/*
 * Golioth User Configuration for Model Updater
 *
 * This file overrides SDK defaults before they are applied in golioth/config.h.
 * Only #define values you want to change from the defaults.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* ---- OTA Configuration ---- */

/*
 * CRITICAL: Default is 1. If your release has 2 artifacts (model + labels),
 * the manifest parser will SILENTLY DROP the second component.
 * Always set this >= the number of artifact types in your release.
 */
#define CONFIG_GOLIOTH_OTA_MAX_NUM_COMPONENTS 4

/* Maximum length of artifact package names (e.g., "model", "labels") */
#define CONFIG_GOLIOTH_OTA_MAX_PACKAGE_NAME_LEN 16

/* Maximum length of version strings (e.g., "1.0.0") */
#define CONFIG_GOLIOTH_OTA_MAX_VERSION_LEN 16

/* How often to poll for manifest updates if push fails (seconds) */
#define CONFIG_GOLIOTH_OTA_MANIFEST_SUBSCRIPTION_POLL_INTERVAL_S 300

/* ---- CoAP / Connection ---- */

/* Max items in the CoAP request queue */
#define CONFIG_GOLIOTH_COAP_REQUEST_QUEUE_MAX_ITEMS 20

/* ---- Logging ---- */

/* Forward logs to Golioth cloud (useful for remote debugging) */
#define CONFIG_GOLIOTH_AUTO_LOG_TO_CLOUD 1

/* Enable debug-level logging */
#define CONFIG_GOLIOTH_DEBUG_LOG

/* ---- Feature Flags ---- */

/* Enable firmware/OTA update support (required for artifact downloads) */
#define CONFIG_GOLIOTH_FW_UPDATE
