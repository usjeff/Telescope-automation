# Telescope-automation
This project is to automate a 4" refractor and goto mount.

The main components are: Raspberry Pi based wireless access point w/ USB GPS, Adafruit motor hat based focuser, Indigo-astronomy focuser driver, and Raspberry HQ camera

The target software is the Indigo-astronomy imager (ain_imager). This recipe has been tested on a Raspberry Pi4 running Debian 
version 1:6.12.75-1+rpt1 (2026-03-11) aarch64 (Trixie).

       Focuser Parts List:
       www.adafruit.com/product/4280         (Stepper motor hat for Raspberry Pi)
       www.adafruit.com/product/324          (NEMA-17 stepper motor)
       www.adafruit.com/product/1297         (NEMA-17 stepper motor mount /w hardware)
       www.adafruit.com/product/1311         (hook-up wire)
       www.adafruit.com/product/344          (heat shrink tubing)
       www.adafruit.com/product/352          (12v switching power supply)

       1" x 2" aluminum angle iron
       2GT timing belt pulley - 20 teeth, 6mm width, 5mm bore 
       2GT timing belt - 6mm width, 164mm length
       Alex Tech Braided cable sleeve

Raspberry Pi 4 setup for Debian trixie server.

       1) Wireless to wired bridge (Losmandy Gemini2)

       configure the RPi4 as a wireless access point for the Gemini 2 
       telescope controller. The RPi4 also proves: NTP server, GPS,
       serial access, RPi HQ camera, and stepper motor focuser.

       sudo apt update
       sudo apt upgrade

       Enable i2c and ssh.

       Install/download the following:

       lshw ethtool dmidecode gpm
       wget wput unzip usbutils net-tools ntpstat dnsutils
       git build-essential autoconf autotools-dev libtool cmake libudev-dev

       chrony gpsd gpsd-tools gpsd-clients 
       python3-gps python3-pip python3-numpy python3-flask

       Use udev to create symbolic links to the gps and telescope USB interfaces. This will allow software to find them
       no mater if they move during enumeration.

       Create fixed paths for the USB gemini2 serial port and u-blox GPS by adding the following udev rule in
       /etc/udev/rules.d/04-gem.rules

       KERNEL=="1-1.[0-9]", SUBSYSTEM=="usb", ATTRS{idVendor}=="1546", ATTRS{idProduct}=="01a7", SYMLINK="usbGPS",GROUP="dialout", MODE="0666"
       KERNEL=="1-1.[0-9]", SUBSYSTEM=="usb", ATTRS{idVendor}=="1fc9", ATTRS{idProduct}=="1705", SYMLINK="usbGEM",GROUP="dialout", MODE="0666" 

       KERNEL=="ttyACM[0-9]*", SUBSYSTEM=="tty", ATTRS{idVendor}=="1546", ATTRS{idProduct}=="01a7", SYMLINK="ttyGPS",GROUP="dialout", MODE="0666"
       KERNEL=="ttyACM[0-9]*", SUBSYSTEM=="tty", ATTRS{idVendor}=="1fc9", ATTRS{idProduct}=="1705", SYMLINK="ttyGEM",GROUP="dialout", MODE="0666"

Install the adafruit motorkit library according the instructions here:
      https://learn.adafruit.com/adafruit-dc-and-stepper-motor-hat-for-raspberry-pi/installing-software
      ie: pip3 install adafruit-circuitpython-motorkit --break-system-packages

      Set the gpsd defaults file at /etc/defaults/gpsd

      # Devices gpsd should connect to at boot time.
      # They need to be read/writeable, either by user gpsd or the group dialout.
      DEVICES="/dev/ttyGPS"

      # Other options you want to pass to gpsd
      GPSD_OPTIONS="-p --nowait --listenany"

      # Automatically hot add/remove USB GPS devices via gpsdctl
      USBAUTO="false"

Enable the gpsd.service via:

  sudo systemctl status gpsd.service
  sudo systemctl enable gpsd.service
  sudo systemctl restart gpsd.service
  sudo systemctl status gpsd.service

NOTE: If gps does not return data after a few minutes; try disconnecting other usb devices. Ssh in
from another system and use gpsmon or cgps to look at gps data. You should have time and location.

The chronyd.service needs to start after the gpsd.service. First we need to switch to the local clock
(gps). Edit /etc/chrony/chrony.conf and comment out the "pool" and "sourcedir" directives. Add these two
lines to the end of the file:

"
    refclock SHM 0 poll 3 delay 0.0 refid SHM0
    allow all
"

Use "sudo systemctl edit --full chrony.service" to add the following to the end of the [Unit] section:

"
    After=gpsd.service
"

Use "sudo systemctl edit --full gpsd.service" to change the [Unit] section:

"
    Before=chronyd.service
"

2) Indigo download and configuration.

Use "git" to download the indigo-astronomy software:

  git clone https://github.com/indigo-astronomy/indigo

follow the directions to make sure you can compile the indigo-astronomy software on your
raspberry pi.

NOTE: The RPi camera driver for indigo-astronomy, ccd_rpi, requires libcamera; that is a seperate build.
See the ccd_rpi README for instructions. 

Download the source for the Adafruit motor hat focuser driver to:

$HOME/indigo/indigo_drivers/focuser_adafruitmh

Edit $HOME/indigo/Makefile and add "focuser_adafruitmh" to the list of STABLE_DRIVERS

   do:
   make all
   sudo make install

If the gemini2, motor hat, and camera are connected the command should execute without error.

$HOME/indigo/build/bin/indigo_server indigo_focuser_adafruit indigo_ccd_rpi indigo_gps_gpsd indigo_mount_lx200 indigo_agent_imager


On a second computer (RPi5), install ain_imager and indigo-control-panel.

sudo apt install ain_imager indigo-control-panel

Launch the imager:
ain_imager <RPi4 IP>

Under the "File" tab select "Manage Services". Ensure "Available Services" has <RPi4 IP>:7624 selected.

You should now be able to select the camera (RPi Camera imx477@1a), focuser (Adafruit Motor Hat Focuser), Mount (lx200), and GPS (GPSD client).













