#!/usr/bin/env python3
"""
camera_infer_tflite.py

Run a TFLite image-classification model on live camera frames and display
predictions. Supports hot-reload of model + labels via SIGUSR1 from the
Golioth model updater daemon.

Usage:
  python3 camera_infer_tflite.py \
      --model .golioth/models/ai-model.tflite \
      --labels .golioth/models/labels.txt \
      --camera 0 --top_k 3

The Golioth model updater daemon will:
  1. Download new model/labels artifacts to .golioth/models/
  2. Update the symlinks (ai-model.tflite, labels.txt)
  3. Send SIGUSR1 to this process (via PID file)
  4. This script reloads model + labels without restarting

Based on: gigwegbe/edge-computing-workshop-japan
"""

import argparse
import signal
import time
import sys
import os

import cv2
import numpy as np
from PIL import Image

# ---- TFLite interpreter import ----
try:
    import tflite_runtime.interpreter as tflite_rt
    Interpreter = tflite_rt.Interpreter
    print("[inference] Using tflite_runtime.Interpreter")
except ImportError:
    try:
        import tensorflow as tf
        Interpreter = tf.lite.Interpreter
        print("[inference] Using tensorflow.lite.Interpreter")
    except ImportError as e:
        raise RuntimeError(
            "Could not import tflite_runtime or tensorflow.lite.Interpreter. "
            "Install with: pip install tflite-runtime"
        ) from e

# ---- PID file for Golioth updater to signal us ----
PID_FILE = "/tmp/golioth_inference.pid"

# ---- Global reload flag (set by SIGUSR1 handler) ----
_reload_requested = False


def on_sigusr1(signum, frame):
    """Signal handler: set flag for main loop to pick up."""
    global _reload_requested
    _reload_requested = True
    print("\n[inference] SIGUSR1 received — model reload requested")


def write_pid_file():
    """Write our PID so the C updater can signal us."""
    try:
        with open(PID_FILE, "w") as f:
            f.write(str(os.getpid()))
        print(f"[inference] PID file written: {PID_FILE} (PID={os.getpid()})")
    except OSError as e:
        print(f"[inference] WARNING: Failed to write PID file: {e}")


def remove_pid_file():
    """Clean up PID file on exit."""
    try:
        os.unlink(PID_FILE)
    except OSError:
        pass


# ---- Model / Labels loading ----

def load_labels(path):
    """Load labels from a text file (one label per line)."""
    if not os.path.exists(path):
        print(f"[inference] WARNING: Labels file not found: {path}")
        return []
    with open(path, "r", encoding="utf-8") as f:
        labels = [line.strip() for line in f.readlines() if line.strip()]
    print(f"[inference] Loaded {len(labels)} labels: {labels}")
    return labels


def make_interpreter(model_path):
    """Load a TFLite model and allocate tensors."""
    if not os.path.exists(model_path):
        print(f"[inference] WARNING: Model file not found: {model_path}")
        return None
    try:
        # Resolve symlink to see actual file
        real_path = os.path.realpath(model_path)
        print(f"[inference] Loading model: {model_path}")
        if real_path != model_path:
            print(f"[inference]   -> resolved to: {real_path}")

        interpreter = Interpreter(model_path=str(model_path))
        interpreter.allocate_tensors()

        input_details = interpreter.get_input_details()[0]
        output_details = interpreter.get_output_details()[0]
        print(f"[inference] Model input shape: {input_details['shape']}, "
              f"dtype: {input_details['dtype']}")
        print(f"[inference] Model output shape: {output_details['shape']}, "
              f"dtype: {output_details['dtype']}")
        return interpreter
    except Exception as e:
        print(f"[inference] ERROR: Failed to load model: {e}")
        return None


# ---- Preprocessing / Inference ----

def preprocess_cv2(frame, input_shape):
    """Convert BGR cv2 frame to RGB array resized to model input shape."""
    img = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    img = Image.fromarray(img)
    img = img.resize((input_shape[2], input_shape[1]), Image.BILINEAR)
    arr = np.asarray(img).astype(np.float32)
    return arr


def set_input_tensor(interpreter, image_array):
    """Set the input tensor with proper dtype handling (float/quantized)."""
    input_details = interpreter.get_input_details()[0]
    tensor_index = input_details["index"]
    input_dtype = input_details["dtype"]
    scale, zero_point = input_details.get("quantization", (0.0, 0))

    if input_dtype == np.float32:
        inp = image_array.astype(np.float32) / 255.0
    else:
        if scale and zero_point is not None:
            inp = image_array.astype(np.float32) / scale + zero_point
            inp = np.round(inp).astype(input_dtype)
        else:
            inp = image_array.astype(input_dtype)

    inp = np.expand_dims(inp, axis=0)
    interpreter.set_tensor(tensor_index, inp)


def get_output_probs(interpreter):
    """Get output probabilities, dequantizing and applying softmax if needed."""
    output_details = interpreter.get_output_details()[0]
    output_data = interpreter.get_tensor(output_details["index"])
    out_dtype = output_details["dtype"]
    scale, zero_point = output_details.get("quantization", (0.0, 0))

    scores = np.squeeze(output_data)
    if out_dtype in (np.uint8, np.int8) and scale:
        scores = scale * (scores.astype(np.float32) - zero_point)

    if scores.min() < 0 or scores.max() > 1 or not np.isclose(scores.sum(), 1.0):
        exps = np.exp(scores - np.max(scores))
        probs = exps / np.sum(exps)
    else:
        probs = scores.astype(np.float32)
    return probs


# ---- Main ----

def main():
    parser = argparse.ArgumentParser(
        description="TFLite Camera Inference with Golioth OTA model reload"
    )
    parser.add_argument("--model", "-m", required=True,
                        help="Path to .tflite / .lite model file")
    parser.add_argument("--labels", "-l", required=True,
                        help="Path to labels.txt (one label per line)")
    parser.add_argument("--camera", type=int, default=0,
                        help="Camera index for cv2.VideoCapture()")
    parser.add_argument("--top_k", type=int, default=1,
                        help="Show top K predictions")
    parser.add_argument("--width", type=int, default=640,
                        help="Capture width")
    parser.add_argument("--height", type=int, default=480,
                        help="Capture height")
    args = parser.parse_args()

    # Setup SIGUSR1 handler and PID file
    signal.signal(signal.SIGUSR1, on_sigusr1)
    write_pid_file()

    global _reload_requested

    try:
        # Initial load
        labels = load_labels(args.labels)
        interpreter = make_interpreter(args.model)

        if interpreter is None:
            print("[inference] No model found. Waiting for Golioth updater to deliver one...")
            print(f"[inference] Watching: {args.model}")
            while interpreter is None:
                if _reload_requested:
                    _reload_requested = False
                    labels = load_labels(args.labels)
                    interpreter = make_interpreter(args.model)
                time.sleep(1)

        input_details = interpreter.get_input_details()[0]
        input_shape = input_details["shape"]

        # Open camera
        cap = cv2.VideoCapture(args.camera)
        if not cap.isOpened():
            print(f"[inference] ERROR: Could not open camera index {args.camera}")
            sys.exit(1)

        cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
        print(f"[inference] Camera {args.camera} opened at {args.width}x{args.height}")
        print("[inference] Press 'q' or ESC to quit")
        print("[inference] Model will auto-reload on SIGUSR1 from Golioth updater\n")

        fps_avg = None
        reload_banner_frames = 0  # show "RELOADED" banner briefly

        while True:
            # ---- Check for reload request ----
            if _reload_requested:
                _reload_requested = False
                print("\n[inference] === RELOADING MODEL AND LABELS ===")

                new_labels = load_labels(args.labels)
                new_interpreter = make_interpreter(args.model)

                if new_interpreter is not None:
                    interpreter = new_interpreter
                    labels = new_labels
                    input_details = interpreter.get_input_details()[0]
                    input_shape = input_details["shape"]
                    reload_banner_frames = 90  # show banner for ~3 seconds at 30fps
                    print("[inference] === RELOAD COMPLETE ===\n")
                else:
                    print("[inference] WARNING: Reload failed, keeping previous model\n")

            # ---- Capture and classify ----
            t0 = time.time()
            ret, frame = cap.read()
            if not ret:
                print("[inference] Failed to read from camera")
                break

            img_arr = preprocess_cv2(frame, input_shape)
            set_input_tensor(interpreter, img_arr)
            interpreter.invoke()
            probs = get_output_probs(interpreter)

            top_idx = np.argsort(probs)[-args.top_k:][::-1]
            predictions = [
                (labels[i] if i < len(labels) else str(i), float(probs[i]))
                for i in top_idx
            ]

            # ---- Overlay predictions ----
            y0 = 30
            for i, (lbl, p) in enumerate(predictions):
                text = f"{lbl}: {p:.2f}"
                cv2.putText(frame, text, (10, y0 + i * 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

            # ---- FPS smoothing ----
            t1 = time.time()
            fps = 1.0 / (t1 - t0) if (t1 - t0) > 0 else 0.0
            fps_avg = fps if fps_avg is None else fps_avg * 0.9 + fps * 0.1
            cv2.putText(frame, f"FPS: {fps_avg:.1f}",
                        (10, frame.shape[0] - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)

            # ---- Reload banner ----
            if reload_banner_frames > 0:
                cv2.putText(frame, "MODEL RELOADED",
                            (10, frame.shape[0] - 40),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
                reload_banner_frames -= 1

            cv2.imshow("TFLite Camera Inference (Golioth OTA)", frame)
            key = cv2.waitKey(1) & 0xFF
            if key in (27, ord("q")):
                break

        cap.release()
        cv2.destroyAllWindows()

    except KeyboardInterrupt:
        print("\n[inference] Interrupted by user")
    finally:
        remove_pid_file()
        print("[inference] Exiting.")


if __name__ == "__main__":
    main()
