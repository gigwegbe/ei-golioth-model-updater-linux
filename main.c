/*
 * Golioth OTA Model Updater for Linux
 *
 * A lightweight daemon that connects to Golioth, watches for new OTA releases
 * containing TFLite model + labels artifacts, downloads them to disk, and
 * signals a Python inference script to reload.
 *
 * Based on: golioth/example-tensorflow-model-update (ESP32)
 * Ported to: Golioth Linux SDK
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#include <golioth/client.h>
#include <golioth/ota.h>
#include <golioth/golioth_sys.h>

/* -------------------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------------------- */

static const char *TAG = "model_updater";

/* Where downloaded models and labels are stored */
#ifndef MODEL_DIR
#define MODEL_DIR "/home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models"
#endif

/* Golioth artifact package names to watch for */
#define PKG_MODEL "ai-model"
#define PKG_LABELS "labels"

/* Maximum number of pending components from a single manifest */
#define MAX_PENDING CONFIG_GOLIOTH_OTA_MAX_NUM_COMPONENTS

/* Path to the Python inference script's PID file (for SIGUSR1 signaling) */
#define INFERENCE_PID_FILE "/tmp/golioth_inference.pid"

/* Path to file that records the currently active version */
#define VERSION_FILE MODEL_DIR "/version.txt"

/* Poll interval in the main loop (milliseconds) */
#define POLL_INTERVAL_MS 1000

/* -------------------------------------------------------------------------
 * Global State
 * ------------------------------------------------------------------------- */

/* Graceful shutdown flag */
static volatile sig_atomic_t g_running = 1;

/* Pending download queue (protected by mutex) */
static pthread_mutex_t g_pending_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct golioth_ota_component *g_pending[MAX_PENDING];
static int g_pending_count = 0;

/* -------------------------------------------------------------------------
 * Signal Handling
 * ------------------------------------------------------------------------- */

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* -------------------------------------------------------------------------
 * Utility: Read PEM File into Heap Buffer
 * ------------------------------------------------------------------------- */

static uint8_t *read_pem_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "[%s] Cannot open %s: %s\n", TAG, path, strerror(errno));
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }

    *out_len = fread(buf, 1, (size_t)sz, f);
    buf[*out_len] = '\0';
    fclose(f);

    return buf;
}

/* -------------------------------------------------------------------------
 * Utility: Directory Creation
 * ------------------------------------------------------------------------- */

static int ensure_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
        {
            return 0;
        }
        fprintf(stderr, "[%s] Path exists but is not a directory: %s\n", TAG, path);
        return -1;
    }

    /* Attempt to create the directory (and parents) */
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);

    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
            {
                fprintf(stderr, "[%s] mkdir failed: %s (%s)\n", TAG, tmp, strerror(errno));
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
    {
        fprintf(stderr, "[%s] mkdir failed: %s (%s)\n", TAG, tmp, strerror(errno));
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * Utility: File Extension for Package Name
 * ------------------------------------------------------------------------- */

static const char *ext_for_package(const char *package)
{
    if (strcmp(package, PKG_MODEL) == 0)
    {
        return ".tflite";
    }
    if (strcmp(package, PKG_LABELS) == 0)
    {
        return ".txt";
    }
    return ".bin";
}

/* -------------------------------------------------------------------------
 * Utility: Check if Package Name is One We Care About
 * ------------------------------------------------------------------------- */

static bool is_our_package(const char *package)
{
    return (strcmp(package, PKG_MODEL) == 0) || (strcmp(package, PKG_LABELS) == 0);
}

/* -------------------------------------------------------------------------
 * Utility: Write Version File
 * ------------------------------------------------------------------------- */

static void write_version_file(const char *version)
{
    FILE *f = fopen(VERSION_FILE, "w");
    if (f)
    {
        fprintf(f, "%s\n", version);
        fclose(f);
        printf("[%s] Updated version file: %s\n", TAG, version);
    }
    else
    {
        fprintf(stderr, "[%s] Failed to write version file: %s\n", TAG, strerror(errno));
    }
}

/* -------------------------------------------------------------------------
 * Utility: Read Stored Version
 * ------------------------------------------------------------------------- */

static int read_version_file(char *buf, size_t buf_size)
{
    FILE *f = fopen(VERSION_FILE, "r");
    if (!f)
    {
        return -1;
    }

    if (!fgets(buf, buf_size, f))
    {
        fclose(f);
        return -1;
    }

    fclose(f);

    /* Strip trailing newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
    {
        buf[len - 1] = '\0';
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * Utility: Signal Python Inference Script to Reload
 * ------------------------------------------------------------------------- */

static void signal_python_reload(void)
{
    FILE *f = fopen(INFERENCE_PID_FILE, "r");
    if (!f)
    {
        printf("[%s] No inference PID file found at %s (Python script may not be running)\n",
               TAG, INFERENCE_PID_FILE);
        return;
    }

    int pid = 0;
    if (fscanf(f, "%d", &pid) == 1 && pid > 0)
    {
        if (kill(pid, SIGUSR1) == 0)
        {
            printf("[%s] Sent SIGUSR1 to Python inference process (PID %d)\n", TAG, pid);
        }
        else
        {
            fprintf(stderr, "[%s] Failed to signal PID %d: %s\n", TAG, pid, strerror(errno));
        }
    }
    else
    {
        fprintf(stderr, "[%s] Failed to read PID from %s\n", TAG, INFERENCE_PID_FILE);
    }

    fclose(f);
}

/* -------------------------------------------------------------------------
 * Utility: Atomic Symlink Update
 *
 * Creates a symlink atomically by:
 *   1. symlink(target, link_path.tmp)
 *   2. rename(link_path.tmp, link_path)
 *
 * rename() is atomic on POSIX filesystems, so the Python script will
 * either see the old symlink or the new one, never a broken state.
 * ------------------------------------------------------------------------- */

static int atomic_symlink(const char *target, const char *link_path)
{
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d", link_path, getpid());

    /* Remove temp if leftover from a previous crash */
    unlink(tmp_path);

    if (symlink(target, tmp_path) != 0)
    {
        fprintf(stderr, "[%s] symlink failed: %s -> %s (%s)\n",
                TAG, tmp_path, target, strerror(errno));
        return -1;
    }

    if (rename(tmp_path, link_path) != 0)
    {
        fprintf(stderr, "[%s] rename failed: %s -> %s (%s)\n",
                TAG, tmp_path, link_path, strerror(errno));
        unlink(tmp_path);
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * Golioth: Connection Event Callback
 * ------------------------------------------------------------------------- */

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    (void)client;
    (void)arg;

    bool connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    printf("[%s] Golioth client %s\n", TAG, connected ? "connected" : "disconnected");
}

/* -------------------------------------------------------------------------
 * Golioth OTA: Manifest Callback
 *
 * Called from the Golioth SDK's CoAP thread whenever a new release
 * manifest is received. We parse the manifest, filter for our package
 * names, and queue matching components for the main thread to download.
 *
 * THREAD SAFETY: This runs on a different thread than main().
 *                All access to g_pending[] is mutex-protected.
 * ------------------------------------------------------------------------- */

static void on_manifest(struct golioth_client *client,
                        enum golioth_status status,
                        const struct golioth_coap_rsp_code *rsp_code,
                        const char *path,
                        const uint8_t *payload,
                        size_t payload_size,
                        void *arg)
{
    (void)client;
    (void)rsp_code;
    (void)path;
    (void)arg;

    if (status != GOLIOTH_OK || !payload || payload_size == 0)
    {
        fprintf(stderr, "[%s] Manifest error (status=%d)\n", TAG, status);
        return;
    }

    struct golioth_ota_manifest man;

    enum golioth_status err = golioth_ota_payload_as_manifest(payload, payload_size, &man);
    if (err)
    {
        fprintf(stderr, "[%s] Failed to parse manifest (err=%d). "
                        "Check CONFIG_GOLIOTH_OTA_MAX_NUM_COMPONENTS (currently %d)\n",
                TAG, err, CONFIG_GOLIOTH_OTA_MAX_NUM_COMPONENTS);
        return;
    }

    printf("[%s] Manifest received: %zu component(s), seqnum=%d\n",
           TAG, man.num_components, man.seqnum);

    pthread_mutex_lock(&g_pending_mutex);

    /* Clear any stale pending items that haven't been processed yet */
    for (int i = 0; i < g_pending_count; i++)
    {
        free(g_pending[i]);
        g_pending[i] = NULL;
    }
    g_pending_count = 0;

    for (size_t i = 0; i < man.num_components; i++)
    {
        printf("[%s]   Component: package=\"%s\" version=\"%s\" size=%d\n",
               TAG, man.components[i].package, man.components[i].version,
               man.components[i].size);

        if (!is_our_package(man.components[i].package))
        {
            printf("[%s]   Skipping (not our package)\n", TAG);
            continue;
        }

        if (g_pending_count >= MAX_PENDING)
        {
            fprintf(stderr, "[%s]   Pending queue full, dropping component\n", TAG);
            continue;
        }

        /* Heap-copy the component for the main thread to process */
        struct golioth_ota_component *copy = malloc(sizeof(struct golioth_ota_component));
        if (!copy)
        {
            fprintf(stderr, "[%s]   Failed to allocate component copy\n", TAG);
            continue;
        }
        memcpy(copy, &man.components[i], sizeof(struct golioth_ota_component));
        g_pending[g_pending_count++] = copy;

        printf("[%s]   Queued for download: %s v%s\n",
               TAG, copy->package, copy->version);
    }

    pthread_mutex_unlock(&g_pending_mutex);
}

/* -------------------------------------------------------------------------
 * Block Context — HEAP allocated so it survives async callbacks
 *
 * The download function allocates this on the heap. The block callback
 * writes through ctx->fd. The end callback closes the file, renames
 * tmp -> final, and frees the ctx.
 * ------------------------------------------------------------------------- */

typedef struct
{
    FILE *fd;
    size_t bytes_written;
    char tmp_path[512];
    char final_path[512];
    char stable_path[512];
    char package[CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN + 1];
    char version[CONFIG_GOLIOTH_OTA_MAX_VERSION_LEN + 1];
} block_ctx_t;

/* -------------------------------------------------------------------------
 * Golioth OTA: Block Write Callback
 * ------------------------------------------------------------------------- */

static enum golioth_status write_block(
    const struct golioth_ota_component *component,
    uint32_t block_idx,
    const uint8_t *block_buffer,
    size_t block_buffer_len,
    bool is_last,
    size_t negotiated_block_size,
    void *arg)
{
    (void)negotiated_block_size;

    block_ctx_t *ctx = (block_ctx_t *)arg;
    if (!ctx || !ctx->fd)
    {
        fprintf(stderr, "[%s] write_block: ctx or fd is NULL\n", TAG);
        return GOLIOTH_ERR_INVALID_FORMAT;
    }

    size_t written = fwrite(block_buffer, 1, block_buffer_len, ctx->fd);
    if (written != block_buffer_len)
    {
        fprintf(stderr, "[%s] write_block: fwrite short (wrote %zu of %zu)\n",
                TAG, written, block_buffer_len);
        return GOLIOTH_ERR_IO;
    }

    ctx->bytes_written += written;

    /* Progress logging every 32 blocks */
    if (block_idx % 32 == 0)
    {
        printf("[%s] Downloaded %zu bytes...\n", TAG, ctx->bytes_written);
    }

    if (is_last)
    {
        printf("[%s] Block download complete for %s v%s (%zu bytes)\n",
               TAG, ctx->package, ctx->version, ctx->bytes_written);
    }

    return GOLIOTH_OK;
}

/* -------------------------------------------------------------------------
 * Golioth OTA: Download End Callback
 *
 * Called exactly once when download finishes. Responsible for:
 *   - Closing the file
 *   - Renaming tmp -> final on success
 *   - Updating the stable symlink
 *   - Writing the version file
 *   - Signaling the Python inference script
 *   - Freeing the heap-allocated ctx
 * ------------------------------------------------------------------------- */

static void on_download_end(
    enum golioth_status status,
    const struct golioth_coap_rsp_code *rsp_code,
    const struct golioth_ota_component *component,
    uint32_t block_idx,
    void *arg)
{
    (void)rsp_code;
    (void)block_idx;

    block_ctx_t *ctx = (block_ctx_t *)arg;
    if (!ctx)
        return;

    /* Close the file handle */
    if (ctx->fd)
    {
        fclose(ctx->fd);
        ctx->fd = NULL;
    }

    if (status != GOLIOTH_OK)
    {
        fprintf(stderr, "[%s] Download FAILED for %s v%s: %s\n",
                TAG, ctx->package, ctx->version,
                golioth_status_to_str(status));
        unlink(ctx->tmp_path);
        free(ctx);
        return;
    }

    printf("[%s] Download complete: %s v%s (%zu bytes)\n",
           TAG, ctx->package, ctx->version, ctx->bytes_written);

    /* Rename tmp -> final (atomic on same filesystem) */
    if (rename(ctx->tmp_path, ctx->final_path) != 0)
    {
        fprintf(stderr, "[%s] Failed to rename %s -> %s: %s\n",
                TAG, ctx->tmp_path, ctx->final_path, strerror(errno));
        unlink(ctx->tmp_path);
        free(ctx);
        return;
    }

    printf("[%s] Saved: %s\n", TAG, ctx->final_path);

    /* Update stable symlink atomically */
    if (atomic_symlink(ctx->final_path, ctx->stable_path) == 0)
    {
        printf("[%s] Updated symlink: %s -> %s\n",
               TAG, ctx->stable_path, ctx->final_path);
    }

    /* Write version file and signal Python */
    write_version_file(ctx->version);
    printf("[%s] Model update complete for %s v%s. Signaling Python script.\n",
           TAG, ctx->package, ctx->version);
    signal_python_reload();

    free(ctx);
}

/* -------------------------------------------------------------------------
 * Download a Single Component to Disk
 *
 * Returns:
 *   0  = download initiated successfully
 *   1  = already exists (skipped)
 *  -1  = error
 * ------------------------------------------------------------------------- */

static int download_component(struct golioth_client *client,
                              const struct golioth_ota_component *component)
{
    const char *ext = ext_for_package(component->package);

    /* Build versioned path: MODEL_DIR/ai-model_2.tflite */
    char vpath[512];
    snprintf(vpath, sizeof(vpath), "%s/%s_%s%s",
             MODEL_DIR, component->package, component->version, ext);

    /* Check if already downloaded */
    struct stat st;
    if (stat(vpath, &st) == 0)
    {
        printf("[%s] Already exists: %s\n", TAG, vpath);

        /* Still ensure symlink is current (e.g., after daemon restart) */
        char stable_path[512];
        snprintf(stable_path, sizeof(stable_path), "%s/%s%s",
                 MODEL_DIR, component->package, ext);
        atomic_symlink(vpath, stable_path);

        return 1;
    }

    /* Build temp path */
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", vpath);

    printf("[%s] Downloading %s v%s (%d bytes) -> %s\n",
           TAG, component->package, component->version, component->size, vpath);

    FILE *f = fopen(tmp_path, "wb");
    if (!f)
    {
        fprintf(stderr, "[%s] Failed to open %s for writing: %s\n",
                TAG, tmp_path, strerror(errno));
        return -1;
    }

    /* Heap-allocate context so it survives across async callbacks.
     * on_download_end is responsible for closing fd and freeing ctx. */
    block_ctx_t *ctx = malloc(sizeof(block_ctx_t));
    if (!ctx)
    {
        fprintf(stderr, "[%s] Failed to allocate block context\n", TAG);
        fclose(f);
        return -1;
    }
    ctx->fd = f;
    ctx->bytes_written = 0;
    snprintf(ctx->tmp_path, sizeof(ctx->tmp_path), "%s", tmp_path);
    snprintf(ctx->final_path, sizeof(ctx->final_path), "%s", vpath);
    snprintf(ctx->stable_path, sizeof(ctx->stable_path), "%s/%s%s",
             MODEL_DIR, component->package, ext);
    snprintf(ctx->package, sizeof(ctx->package), "%s", component->package);
    snprintf(ctx->version, sizeof(ctx->version), "%s", component->version);

    enum golioth_status status = golioth_ota_download_component(
        client,
        component,
        0,               /* block_idx: start from beginning */
        write_block,     /* block callback */
        on_download_end, /* end callback: closes file, renames, frees ctx */
        ctx              /* heap-allocated context */
    );

    if (status != GOLIOTH_OK)
    {
        fprintf(stderr, "[%s] download_component call failed for %s: %s\n",
                TAG, component->package, golioth_status_to_str(status));
        /* If the call itself failed, on_download_end won't be called,
         * so we must clean up here. */
        if (ctx->fd)
        {
            fclose(ctx->fd);
            ctx->fd = NULL;
        }
        unlink(tmp_path);
        free(ctx);
        return -1;
    }

    /* If status == GOLIOTH_OK, on_download_end handles cleanup */
    return 0;
}

/* -------------------------------------------------------------------------
 * Process All Pending Downloads
 *
 * Takes a snapshot of the pending queue, clears it, then kicks off
 * downloads. The on_download_end callback handles rename, symlink
 * update, version file, and Python signaling when each download
 * completes asynchronously.
 * ------------------------------------------------------------------------- */

static void process_pending_downloads(struct golioth_client *client)
{
    /* Snapshot and clear the pending queue under lock */
    pthread_mutex_lock(&g_pending_mutex);

    if (g_pending_count == 0)
    {
        pthread_mutex_unlock(&g_pending_mutex);
        return;
    }

    struct golioth_ota_component *local[MAX_PENDING];
    int count = g_pending_count;

    for (int i = 0; i < count; i++)
    {
        local[i] = g_pending[i];
        g_pending[i] = NULL;
    }
    g_pending_count = 0;

    pthread_mutex_unlock(&g_pending_mutex);

    /* Kick off each download.
     * on_download_end handles: close file, rename, symlink, version, signal. */
    printf("[%s] Processing %d pending download(s)\n", TAG, count);

    for (int i = 0; i < count; i++)
    {
        download_component(client, local[i]);
        free(local[i]);
    }
}

/* -------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("[%s] ========================================\n", TAG);
    printf("[%s] Golioth OTA Model Updater for Linux\n", TAG);
    printf("[%s] Model directory: %s\n", TAG, MODEL_DIR);
    printf("[%s] ========================================\n", TAG);

    /* Setup signal handlers for graceful shutdown */
    setup_signals();

    /* Ensure model directory exists */
    if (ensure_directory(MODEL_DIR) != 0)
    {
        fprintf(stderr, "[%s] Failed to create model directory: %s\n", TAG, MODEL_DIR);
        return 1;
    }

    /* Print current version if available */
    char current_version[64];
    if (read_version_file(current_version, sizeof(current_version)) == 0)
    {
        printf("[%s] Current model version: %s\n", TAG, current_version);
    }
    else
    {
        printf("[%s] No model version found. Waiting for first release.\n", TAG);
    }

/* ---- Golioth credentials: Certificate Auth (PKI) ----
 *
 * Reads PEM files from the certs/ directory.
 * Adjust paths as needed for your deployment.
 */
#define CLIENT_CERT_PATH "certs/client.crt.pem"
#define CLIENT_KEY_PATH "certs/client.key.pem"
#define SERVER_CA_PATH "isrgrootx1_goliothrootx1.pem"

    size_t ca_len = 0, cert_len = 0, key_len = 0;
    uint8_t *ca_buf = read_pem_file(SERVER_CA_PATH, &ca_len);
    uint8_t *cert_buf = read_pem_file(CLIENT_CERT_PATH, &cert_len);
    uint8_t *key_buf = read_pem_file(CLIENT_KEY_PATH, &key_len);

    if (!ca_buf || !cert_buf || !key_buf)
    {
        fprintf(stderr, "\n[%s] ERROR: Certificate files missing.\n", TAG);
        fprintf(stderr, "[%s] Need:\n", TAG);
        fprintf(stderr, "[%s]   %s\n", TAG, SERVER_CA_PATH);
        fprintf(stderr, "[%s]   %s\n", TAG, CLIENT_CERT_PATH);
        fprintf(stderr, "[%s]   %s\n", TAG, CLIENT_KEY_PATH);
        free(ca_buf);
        free(cert_buf);
        free(key_buf);
        return 1;
    }

    printf("[%s] Certs loaded: CA=%zuB cert=%zuB key=%zuB\n",
           TAG, ca_len, cert_len, key_len);

    /* ---- Create Golioth client ---- */
    struct golioth_client_config config = {
        .credentials = {
            .auth_type = GOLIOTH_TLS_AUTH_TYPE_PKI,
            .pki = {
                .ca_cert = ca_buf,
                .ca_cert_len = ca_len,
                .public_cert = cert_buf,
                .public_cert_len = cert_len,
                .private_key = key_buf,
                .private_key_len = key_len,
            },
        },
    };

    struct golioth_client *client = golioth_client_create(&config);
    if (!client)
    {
        fprintf(stderr, "[%s] Failed to create Golioth client\n", TAG);
        return 1;
    }

    /* Register event callback for logging */
    golioth_client_register_event_callback(client, on_client_event, NULL);

    /* Wait for connection */
    printf("[%s] Connecting to Golioth...\n", TAG);
    bool connected = golioth_client_wait_for_connect(client, 30000);
    if (!connected)
    {
        fprintf(stderr, "[%s] Failed to connect to Golioth (timeout)\n", TAG);
        golioth_client_destroy(client);
        return 1;
    }

    printf("[%s] Connected to Golioth.\n", TAG);

    /* ---- Subscribe to OTA manifest updates ----
     *
     * NOTE: The old ESP32 API used golioth_ota_observe_manifest_async().
     *       That function has been REMOVED in the current SDK.
     *       Use golioth_ota_manifest_subscribe() instead.
     *       Same callback signature, different function name.
     */
    enum golioth_status err = golioth_ota_manifest_subscribe(client, on_manifest, NULL);
    if (err != GOLIOTH_OK)
    {
        fprintf(stderr, "[%s] Failed to subscribe to manifest: %s\n",
                TAG, golioth_status_to_str(err));
        golioth_client_destroy(client);
        return 1;
    }

    printf("[%s] Subscribed to OTA manifest. Waiting for releases...\n", TAG);

    /* ---- Main loop ---- */
    while (g_running)
    {
        process_pending_downloads(client);
        golioth_sys_msleep(POLL_INTERVAL_MS);
    }

    /* ---- Graceful shutdown ---- */
    printf("\n[%s] Shutting down...\n", TAG);
    golioth_client_stop(client);
    golioth_client_destroy(client);
    free(ca_buf);
    free(cert_buf);
    free(key_buf);
    printf("[%s] Goodbye.\n", TAG);

    return 0;
}