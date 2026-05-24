#!/usr/bin/env python3

##
## Test web interface for HQ camera and Adafruit Motor Hat access.
##

from flask import Flask, render_template, redirect, url_for
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
    regx = r'[+-]?\d+'  #optional leading + or - sign 

app.url_map.converters['signed_int'] = SignedIntConverter

def take_photo():
    # Capture with rpicam-still
    cmd = [
        "rpicam-still",
        "-o", IMAGE_PATH,
        "--nopreview",
        "--immediate", "1",
        "-t", "1",
        "--width", "1280",
        "--height", "720",
        "vflip=1", "--hflip=1"
    ]
    subprocess.run(cmd, check=True)
    # Touch file so browsers don't cache it.
    os.utime(IMAGE_PATH, None)

@app.route("/")
def index():
    # Append timestamp to bypass caching.
    ts = int(time.time())
    return render_template("index.html", ts=ts)

@app.route("/capture")
def capture():
    take_photo()
    return redirect(url_for("index"))

def focus(count):
   import time    
  
   count = int(count)
 
   loop = abs(count)

   for i in range(loop):
      if count > 0:
         kit.stepper1.onestep(direction=stepper.FORWARD, style=stepper.DOUBLE)
      elif count < 0:
         kit.stepper1.onestep(direction=stepper.BACKWARD, style=stepper.DOUBLE)

      time.sleep(0.01)

   kit.stepper1.release()

   return()

@app.route("/focus/<signed_int:count>")
def set_focus(count):
    focus(count)    # blocks for mechanical motion
    return redirect(url_for("index"))

#@app.route("/debug/<int:n>")
#def debug(n):
#    print("DEBUG:", n)
#    return f"OK: {n}"

#@app.route("/echo/<path:p>")
#def echo(p):
#    print("ECHO RAW:", repr(p))
#    return f"RAW: {repr(p)}"

#@app.route("/focus/<path:p>")
#def focus_catch(p):
#    print("CAUGHT IN PATH ROUTE:", repr(p))
#    return "path caught: " + repr(p)



if __name__ == "__main__":
    # Listen on just one LAN interface 
    app.run(host="192.178.0.240", port=5000, debug=False)


 
