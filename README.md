# ei-golioth-model-updater-linux

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
-
