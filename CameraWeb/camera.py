#!/usr/bin/env python3
##
## Web interface for HQ camera and Adafruit Motor Hat access.
## Supports runtime rpicam-still parameter adjustment via /set_params route.
##
from flask import Flask, render_template, redirect, url_for, request, jsonify
import subprocess
import time
import os
import board
from werkzeug.routing import BaseConverter
from adafruit_motor import stepper
from adafruit_motorkit import MotorKit

kit = MotorKit(i2c=board.I2C())
app = Flask(__name__)
IMAGE_PATH = "static/latest.jpg"

class SignedIntConverter(BaseConverter):
    regx = r'[+-]?\d+'  # optional leading + or - sign

app.url_map.converters['signed_int'] = SignedIntConverter

# ── Camera parameter state (defaults match original hard-coded values) ──────────
cam_params = {
    "width":      "1280",
    "height":     "720",
    "shutter":    "10us",
    "gain":       "1",
    "awbgains":   "1,1",
    "metering":   "centre",
    "ev":         "0",
    "brightness": "1",
    "hdr":        "off",
    "hflip":      "1",
    "vflip":      "1",
}

def take_photo():
    p = cam_params   # shorthand

    cmd = [
        "rpicam-still",
        "-o", IMAGE_PATH,
        "--nopreview",
        "--immediate", "1",
        "-t", "1",
        "--width",    p["width"],
        "--height",   p["height"],
        f"--vflip={p['vflip']}",  f"--hflip={p['hflip']}",   # positional flags
        f"--hdr={p['hdr']}",
        f"--awbgains={p['awbgains']}",
        f"--gain={p['gain']}",
        f"--metering={p['metering']}",
        f"--ev={p['ev']}",
        f"--shutter={p['shutter']}",
        "--flush=1",
        f"--brightness={p['brightness']}",
    ]
    subprocess.run(cmd, check=True)
    # Touch file so browsers don't cache it.
    os.utime(IMAGE_PATH, None)


@app.route("/")
def index():
    ts = int(time.time())
    return render_template("index.html", ts=ts, cam_params=cam_params)


@app.route("/capture")
def capture():
    take_photo()
    return redirect(url_for("index"))


@app.route("/set_params", methods=["POST"])
def set_params():
    """Receive JSON camera parameters from the UI and update the global state."""
    data = request.get_json(force=True)
    allowed = set(cam_params.keys())
    for key, value in data.items():
        if key in allowed:
            cam_params[key] = str(value)
    return jsonify({"status": "ok", "params": cam_params})


def focus(count):
    count = int(count)
    loop = abs(count)
    for i in range(loop):
        if count > 0:
            kit.stepper1.onestep(direction=stepper.FORWARD, style=stepper.DOUBLE)
        elif count < 0:
            kit.stepper1.onestep(direction=stepper.BACKWARD, style=stepper.DOUBLE)
        time.sleep(0.01)
    kit.stepper1.release()


@app.route("/focus/<signed_int:count>")
def set_focus(count):
    focus(count)
    return redirect(url_for("index"))


if __name__ == "__main__":
    app.run(host="192.178.0.240", port=5000, debug=False)
