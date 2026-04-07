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

## TODO

- Update model path (MODEL_DIR) in the main.c
-
