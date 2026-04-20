<!-- # ei-golioth-model-updater-linux

Deploy and update Edge Impulse models on Linux edge devices over-the-air using Golioth — zero-downtime hot-reload for image classification.

# Edge Impulse Golioth OTA Model Updater for Linux

A lightweight C daemon that connects to Golioth, watches for new OTA releases
containing TFLite model artifacts, downloads them to disk, and signals
a Python camera inference script to hot-reload.

## Architecture

```
┌──────────────┐         ┌─────────────────────┐         ┌──────────────────────┐
│  Golioth     │  CoAP   │  C Model Updater    │ SIGUSR1 │  Python Camera       │
│  Cloud       │────────>│  (golioth_model_     │───────>│  Inference           │
│              │         │   updater)           │        │  (camera_infer_      │
│  - Artifacts │         │                     │         │   tflite.py)         │
│  - Releases  │         │  - Subscribe to OTA │         │                      │
│  - Rollouts  │         │  - Download model   │         │  - Load .tflite      │
│              │         │  - Write to disk    │         │  - Live camera feed  │
│              │         │  - Signal Python    │         │  - Classify frames   │
└──────────────┘         └──────┬──────────────┘         └────────┬─────────────┘
                                │                                 │
                                ▼                                 ▼
                    ~/.golioth/models/               ~/.golioth/models/
                    ├── ai-model.tflite (symlink)    ai-model_3.tflite
                    ├── labels.txt                   (reads stable symlink)
                    └── version.txt
```

## Prerequisites

```bash
# System dependencies (C daemon)
sudo apt install build-essential cmake libcoap3-dev libssl-dev

# Python dependencies (inference script)
pip install -r requirements.txt

# Clone the Golioth Firmware SDK (with submodules)
git clone https://github.com/golioth/golioth-firmware-sdk.git
cd golioth-firmware-sdk && git submodule update --init --recursive
```

Initial Connection once built:

```bash
ubuntu@ubuntu:~/Documents/ei-golioth-model-updater-linux$ ./build/ei-golioth-model-updater-linux
[model_updater] ========================================
[model_updater] Golioth OTA Model Updater for Linux
[model_updater] Model directory: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models
[model_updater] ========================================
[model_updater] No model version found. Waiting for first release.
[model_updater] Certs loaded: CA=2737B cert=534B key=227B
I (0) golioth_mbox: Mbox created, bufsize: 3528, num_items: 20, item_size: 168
[model_updater] Connecting to Golioth...
I (118) golioth_coap_client_libcoap: Start CoAP session with host: coaps://coap.golioth.io
I (121) golioth_coap_client_libcoap: Entering CoAP I/O loop
I (727) libcoap:    192.168.1.74:56350 <-> 34.135.90.112:5684 DTLS: unable to get certificate CRL: overridden: 'coap.golioth.io' depth=0

I (727) golioth_coap_client_libcoap: Server Cert: Depth = 2, Len = 1391, Valid = 1
I (728) golioth_coap_client_libcoap: Server Cert: Depth = 1, Len = 1114, Valid = 1
I (729) golioth_coap_client_libcoap: Server Cert: Depth = 0, Len = 912, Valid = 1
I (1372) golioth_coap_client_libcoap: Golioth CoAP client connected
[model_updater] Golioth client connected
[model_updater] Connected to Golioth.
[model_updater] Subscribed to OTA manifest. Waiting for releases...
[model_updater] Manifest received: 0 component(s), seqnum=0
```

Initial Model:

```bash

(729) golioth_coap_client_libcoap: Server Cert: Depth = 0, Len = 912, Valid = 1
I (1372) golioth_coap_client_libcoap: Golioth CoAP client connected
[model_updater] Golioth client connected
[model_updater] Connected to Golioth.
[model_updater] Subscribed to OTA manifest. Waiting for releases...
[model_updater] Manifest received: 0 component(s), seqnum=0
golioth.crt.pem[model_updater] Manifest received: 0 component(s), seqnum=0
[model_updater] Manifest received: 0 component(s), seqnum=0
[model_updater] Manifest received: 0 component(s), seqnum=0
[model_updater] Manifest received: 2 component(s), seqnum=1238575637
[model_updater]   Component: package="labels" version="1" size=30
[model_updater]   Queued for download: labels v1
[model_updater]   Component: package="ai-model" version="1" size=624000
[model_updater]   Queued for download: ai-model v1
[model_updater] Manifest received: 2 component(s), seqnum=1238575637
[model_updater]   Component: package="labels" version="1" size=30
[model_updater]   Queued for download: labels v1
[model_updater]   Component: package="ai-model" version="1" size=624000
[model_updater]   Queued for download: ai-model v1
[model_updater] Processing 2 pending download(s)
[model_updater] Downloading labels v1 (30 bytes) -> /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Downloading ai-model v1 (624000 bytes) -> /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
[model_updater] Downloaded 30 bytes...
[model_updater] Block download complete for labels v1 (30 bytes)
[model_updater] Download complete: labels v1 (30 bytes)
[model_updater] Saved: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Updated symlink: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels.txt -> /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Updated version file: 1
[model_updater] Model update complete for labels v1. Signaling Python script.
[model_updater] No inference PID file found at /tmp/golioth_inference.pid (Python script may not be running)
[model_updater] Downloaded 1024 bytes...
[model_updater] Downloaded 33792 bytes...
[model_updater] Downloaded 66560 bytes...
[model_updater] Downloaded 99328 bytes...
[model_updater] Downloaded 132096 bytes...
[model_updater] Manifest received: 2 component(s), seqnum=372177879
[model_updater]   Component: package="labels" version="1" size=30
[model_updater]   Queued for download: labels v1
[model_updater]   Component: package="ai-model" version="1" size=624000
[model_updater]   Queued for download: ai-model v1
[model_updater] Downloaded 164864 bytes...
[model_updater] Processing 2 pending download(s)
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Downloading ai-model v1 (624000 bytes) -> /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
[model_updater] Downloaded 1024 bytes...
[model_updater] Downloaded 197632 bytes...
[model_updater] Downloaded 33792 bytes...
W (1288355) golioth_coap_client_libcoap: CoAP message retransmitted
[model_updater] Downloaded 230400 bytes...
[model_updater] Downloaded 66560 bytes...
[model_updater] Downloaded 263168 bytes...
[model_updater] Downloaded 99328 bytes...
[model_updater] Downloaded 295936 bytes...
[model_updater] Downloaded 132096 bytes...
[model_updater] Downloaded 328704 bytes...
[model_updater] Downloaded 164864 bytes...
[model_updater] Downloaded 361472 bytes...
[model_updater] Downloaded 197632 bytes...
[model_updater] Downloaded 394240 bytes...
[model_updater] Downloaded 230400 bytes...
[model_updater] Downloaded 427008 bytes...
[model_updater] Downloaded 263168 bytes...
[model_updater] Downloaded 459776 bytes...
[model_updater] Downloaded 295936 bytes...
[model_updater] Downloaded 492544 bytes...
[model_updater] Downloaded 328704 bytes...
[model_updater] Downloaded 525312 bytes...
[model_updater] Downloaded 361472 bytes...
[model_updater] Downloaded 558080 bytes...
[model_updater] Downloaded 394240 bytes...
[model_updater] Manifest received: 2 component(s), seqnum=372177879
[model_updater]   Component: package="labels" version="1" size=30
[model_updater]   Queued for download: labels v1
[model_updater]   Component: package="ai-model" version="1" size=624000
[model_updater]   Queued for download: ai-model v1
[model_updater] Processing 2 pending download(s)
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Downloading ai-model v1 (624000 bytes) -> /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
[model_updater] Downloaded 1024 bytes...
[model_updater] Downloaded 590848 bytes...
[model_updater] Downloaded 427008 bytes...
[model_updater] Downloaded 33792 bytes...
[model_updater] Downloaded 623616 bytes...
[model_updater] Block download complete for ai-model v1 (624000 bytes)
[model_updater] Download complete: ai-model v1 (624000 bytes)
[model_updater] Saved: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
[model_updater] Updated symlink: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model.tflite -> /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
[model_updater] Updated version file: 1
[model_updater] Model update complete for ai-model v1. Signaling Python script.
[model_updater] No inference PID file found at /tmp/golioth_inference.pid (Python script may not be running)
[model_updater] Downloaded 459776 bytes...
[model_updater] Downloaded 66560 bytes...
[model_updater] Downloaded 492544 bytes...
[model_updater] Downloaded 99328 bytes...
[model_updater] Downloaded 525312 bytes...
[model_updater] Downloaded 132096 bytes...
[model_updater] Downloaded 558080 bytes...
[model_updater] Downloaded 164864 bytes...
[model_updater] Downloaded 590848 bytes...
[model_updater] Downloaded 197632 bytes...
[model_updater] Downloaded 623616 bytes...
[model_updater] Block download complete for ai-model v1 (624000 bytes)
[model_updater] Download complete: ai-model v1 (624000 bytes)
[model_updater] Failed to rename /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite.tmp -> /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite: No such file or directory
[model_updater] Downloaded 230400 bytes...
[model_updater] Downloaded 263168 bytes...
[model_updater] Downloaded 295936 bytes...
[model_updater] Downloaded 328704 bytes...
[model_updater] Downloaded 361472 bytes...
[model_updater] Downloaded 394240 bytes...
[model_updater] Downloaded 427008 bytes...
[model_updater] Downloaded 459776 bytes...
[model_updater] Downloaded 492544 bytes...
[model_updater] Downloaded 525312 bytes...
[model_updater] Downloaded 558080 bytes...
[model_updater] Downloaded 590848 bytes...
[model_updater] Downloaded 623616 bytes...
[model_updater] Block download complete for ai-model v1 (624000 bytes)
[model_updater] Download complete: ai-model v1 (624000 bytes)
[model_updater] Failed to rename /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite.tmp -> /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite: No such file or directory
[model_updater] Manifest received: 2 component(s), seqnum=372177879
[model_updater]   Component: package="labels" version="1" size=30
[model_updater]   Queued for download: labels v1
[model_updater]   Component: package="ai-model" version="1" size=624000
[model_updater]   Queued for download: ai-model v1
[model_updater] Processing 2 pending download(s)
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
[model_updater] Manifest received: 2 component(s), seqnum=372177879
[model_updater]   Component: package="labels" version="1" size=30
[model_updater]   Queued for download: labels v1
[model_updater]   Component: package="ai-model" version="1" size=624000
[model_updater]   Queued for download: ai-model v1
[model_updater] Processing 2 pending download(s)
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
[model_updater] Manifest received: 2 component(s), seqnum=372177879
[model_updater]   Component: package="labels" version="1" size=30
[model_updater]   Queued for download: labels v1
[model_updater]   Component: package="ai-model" version="1" size=624000
[model_updater]   Queued for download: ai-model v1
[model_updater] Processing 2 pending download(s)
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
[model_updater] Manifest received: 2 component(s), seqnum=372177879
[model_updater]   Component: package="labels" version="1" size=30
[model_updater]   Queued for download: labels v1
[model_updater]   Component: package="ai-model" version="1" size=624000
[model_updater]   Queued for download: ai-model v1
[model_updater] Processing 2 pending download(s)
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
[model_updater] Manifest received: 2 component(s), seqnum=372177879
[model_updater]   Component: package="labels" version="1" size=30
[model_updater]   Queued for download: labels v1
[model_updater]   Component: package="ai-model" version="1" size=624000
[model_updater]   Queued for download: ai-model v1
[model_updater] Processing 2 pending download(s)
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/labels_1.txt
[model_updater] Already exists: /home/ubuntu/Documents/ei-golioth-model-updater-linux/.golioth/models/ai-model_1.tflite
```

## TODO

- Update model path (MODEL_DIR) in the main.c
- -->

# Edge Impulse + Golioth OTA Model Updater for Linux

Deploy and update Edge Impulse models on Linux edge devices over-the-air using Golioth. Zero-downtime hot-reload for image classification.

## Overview

A two-process system for delivering ML model updates to Linux edge devices:

1. **C Model Updater Daemon** connects to Golioth, watches for new releases, downloads model + labels artifacts to disk, and signals the inference script to reload.
2. **Python Camera Inference Script** runs real-time TFLite image classification on a live camera feed and hot-reloads the model on SIGUSR1 without restarting.

```
┌──────────────┐         ┌─────────────────────┐         ┌──────────────────────┐
│  Golioth     │  CoAP   │  C Model Updater    │ SIGUSR1 │  Python Camera       │
│  Cloud       │────────▶│  Daemon             │────────▶│  Inference           │
│              │  DTLS   │                     │         │                      │
│  Packages    │         │  Subscribe to OTA   │         │  Load .tflite        │
│  Cohorts     │         │  Download artifacts │         │  Live camera feed    │
│  Deployments │         │  Atomic symlink swap│         │  Classify frames     │
└──────────────┘         └──────┬──────────────┘         └────────┬─────────────┘
                                │                                  │
                                ▼                                  ▼
                      .golioth/models/                   Reads from stable
                      ├── ai-model.tflite (symlink)      symlink paths
                      ├── labels.txt      (symlink)
                      └── version.txt
```

## Hardware

- [Qualcomm Thundercomm RUBIK Pi 3](https://rubikpi.ai/) (or any Linux SBC)
- USB Webcam (Logitech HD Pro or similar)

## Software and Services

- [Edge Impulse Studio](https://studio.edgeimpulse.com/) for model training and export
- [Golioth](https://golioth.io/) for OTA artifact management and device fleet management
- [Golioth Firmware SDK](https://github.com/golioth/golioth-firmware-sdk) (Linux port) for CoAP/DTLS connectivity

## Project Structure

```
ei-golioth-model-updater-linux/
├── main.c                      # C daemon: Golioth OTA + file management
├── golioth_user_config.h       # SDK configuration overrides
├── CMakeLists.txt              # Build configuration
├── camera_infer_tflite.py      # Python camera inference with SIGUSR1 reload
├── requirements.txt            # Python dependencies
├── certs/                      # Device certificates (PKI auth)
│   ├── golioth.crt.pem         # Project CA (upload to Golioth console)
│   ├── golioth.key.pem         # Project CA key (keep safe!)
│   ├── client.crt.pem          # Device certificate
│   └── client.key.pem          # Device private key
├── isrgrootx1_goliothrootx1.pem # Golioth server root CA
└── .golioth/models/            # Downloaded models (created at runtime)
    ├── ai-model.tflite         # Symlink to latest model version
    ├── labels.txt              # Symlink to latest labels version
    └── version.txt             # Current version string
```

## Prerequisites

```bash
# System dependencies
sudo apt install build-essential cmake libcoap3-dev libssl-dev
 
# Python dependencies
pip install -r requirements.txt
```

## Full Setup

### Step 0: Clone the Golioth Firmware SDK

```bash
git clone https://github.com/golioth/golioth-firmware-sdk.git
cd golioth-firmware-sdk
git submodule update --init --recursive
cd ..
```

### Step 1: Generate Project Root CA (One Time Per Project)

```bash
cd ei-golioth-model-updater-linux
mkdir -p certs && cd certs
../../golioth-firmware-sdk/scripts/certificates/generate_root_certificate.sh
```

Creates:

- `golioth.crt.pem` — upload this to Golioth console
- `golioth.key.pem` — **keep this safe**

### Step 2: Upload CA to Golioth Console

1. Go to [console.golioth.io](https://console.golioth.io)
2. **Project Settings** → **Certificates** tab
3. **Add CA Certificate** → type = **Root** → upload `golioth.crt.pem`

### Step 3: Generate Device Certificate

```bash
# Still inside certs/
../../golioth-firmware-sdk/scripts/certificates/generate_device_certificate.sh <project_id> <certificate_id> pem
```

- `project_id`: found on the Golioth console → **Projects** → **Project ID** column
- `certificate_id`: a unique identifier for this device
Example:

```bash
../../golioth-firmware-sdk/scripts/certificates/generate_device_certificate.sh ei-model-deployment rubik-pi pem
```

### Step 4: Copy to Standard Names

```bash
cp ei-model-deployment-rubik-pi.crt.pem client.crt.pem
cp ei-model-deployment-rubik-pi.key.pem client.key.pem
cd ..
```

### Step 5: Get the Golioth Server Root CA

```bash
cp ../golioth-firmware-sdk/src/isrgrootx1_goliothrootx1.pem .
```

### Step 6: Build

```bash
mkdir build && cd build
cmake -DGOLIOTH_SDK_PATH=../../golioth-firmware-sdk ..
make -j$(nproc)
cd ..
```

## Running

### Terminal 1: Start the model updater

```bash
./build/ei-golioth-model-updater-linux
```

Expected output on first run (no model yet):

```
[model_updater] ========================================
[model_updater] Golioth OTA Model Updater for Linux
[model_updater] ========================================
[model_updater] No model version found. Waiting for first release.
[model_updater] Certs loaded: CA=2737B cert=534B key=227B
[model_updater] Connecting to Golioth...
[model_updater] Golioth client connected
[model_updater] Subscribed to OTA manifest. Waiting for releases...
```

### Terminal 2: Start the Python camera inference

```bash
QT_QPA_PLATFORM=xcb python3 camera_infer_tflite.py \
    --model .golioth/models/ai-model.tflite \
    --labels .golioth/models/labels.txt \
    --camera 0 --top_k 3
```

If no model exists yet, the script waits:

```
[inference] No model found. Waiting for Golioth updater to deliver one...
```

## Deploying Models via Golioth Console

### 1. Create Packages

Go to **Packages** → **Create** and create two packages:

| Package Name | Description |
|-------------|-------------|
| `ai-model` | Edge Impulse TFLite image classification model |
| `labels` | Classification labels for the ai-model package |

### 2. Upload Versions

Open each package and click **New Version**:

- `ai-model` → version `1` → upload your `.tflite` file
- `labels` → version `1` → upload your `labels.txt` file

### 3. Create a Cohort

Go to **Cohorts** → **Create Cohort** → name it `default` (or `dev`).

Add your device to the cohort:

- **From the cohort page:** Click **Add Devices** → find your device → click **Add**
- **From the Device Index:** Select device → **Bulk Actions** → **Assign to cohort**

### 4. Deploy

Select your cohort → **Deploy** → select both packages:

- `ai-model` → version `1`
- `labels` → version `1`
Click **Next** → review changes → **Start Deployment**.

The model updater downloads both artifacts:

```
[model_updater] Manifest received: 2 component(s)
[model_updater]   Queued for download: labels v1
[model_updater]   Queued for download: ai-model v1
[model_updater] Download complete: labels v1 (30 bytes)
[model_updater] Download complete: ai-model v1 (624000 bytes)
[model_updater] Model update complete. Signaling Python script.
```

Python reloads and starts classifying:

```
[inference] SIGUSR1 received — model reload requested
[inference] === RELOADING MODEL AND LABELS ===
[inference] Loaded 2 labels: ['bell-pepper', 'oranges']
[inference] Model input shape: [1 320 320 3], dtype: int8
[inference] === RELOAD COMPLETE ===
```

### 5. Pushing an Updated Model

When you have a retrained model:

1. **Packages** → `ai-model` → **New Version** → upload as version `2`
2. **Cohorts** → your cohort → **Deploy** → select `ai-model` v2 + `labels` v1
3. Click **Start Deployment**
The updater downloads only the new model (labels v1 already exists on disk and is skipped), swaps the symlink, and signals Python. A "MODEL RELOADED" banner appears on the camera feed and inference continues with the updated model.

## How the Reload Works

1. Golioth pushes a new manifest to the C daemon
2. C daemon downloads artifacts to versioned files (e.g., `ai-model_2.tflite`)
3. C daemon atomically swaps the stable symlink (`ai-model.tflite` → `ai-model_2.tflite`)
4. C daemon sends SIGUSR1 to the Python process via PID file
5. Python signal handler sets a reload flag
6. On the next frame loop iteration, Python reloads model + labels
7. A "MODEL RELOADED" banner appears on the camera feed for ~3 seconds
8. Inference continues with the new model. Zero downtime.

## Configuration

Key settings in `golioth_user_config.h`:

```c
// Must be >= number of artifact types per release.
// Default is 1, which silently drops the second artifact!
#define CONFIG_GOLIOTH_OTA_MAX_NUM_COMPONENTS 4
```

Key defines in `main.c`:

```c
#define PKG_MODEL   "ai-model"         // Golioth package name for the model
#define PKG_LABELS  "labels"           // Golioth package name for labels
#define MODEL_DIR   ".golioth/models"  // Where artifacts are stored on disk
```

## References

- [Golioth OTA: Managing Packages](https://docs.golioth.io/device-management/ota/managing-packages)
- [Golioth OTA: Managing Cohorts](https://docs.golioth.io/device-management/ota/managing-cohorts)
- [Golioth OTA: Deploying Updates](https://docs.golioth.io/device-management/ota/deploying-updates)
- [Golioth Firmware SDK](https://github.com/golioth/golioth-firmware-sdk)
- [Golioth Linux Certificate Auth Example](https://github.com/golioth/golioth-firmware-sdk/tree/main/examples/linux/certificate_auth)
- [Edge Impulse Documentation](https://docs.edgeimpulse.com/)
- [Edge Computing Workshop Japan](https://github.com/gigwegbe/edge-computing-workshop-japan) (Python inference base)
