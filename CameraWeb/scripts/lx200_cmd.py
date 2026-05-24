#!/usr/bin/env python3

# Create a symbolic link to your Combined_Constellation_Stars.guc file in
# the local directory in order to use the "goto" command.
# ln -s $HOME/catalogs/Combined_Constellation_Stars.guc catalog
#
# This code assumes you have /dev/ttyGEM and /dev/ttyGPS defined
#

import argparse
import serial
import time
import gps
import math
from datetime import datetime

class GPSReceiver:
    def __init__(self):
        self.gpsd = gps.gps(mode=gps.WATCH_ENABLE)  # Enable GPS data

    def get_location(self):
        # Grab GPS data
        for _ in range(10):  # check a few reports
            report = self.gpsd.next()
            if report['class'] == 'TPV':  # TPV = Time-Position-Velocity
               lat = report.get('lat', None)
               lon = report.get('lon', None)
               utc = report.get('time', None) 
               return lat, lon, utc

        return None, None, None

def decimal_to_dms(decimal_degrees):
    sign = -1 if decimal_degrees < 0 else 1
    abs_degrees = abs(decimal_degrees)
    degrees = int(abs_degrees)

    minutes_decimal = (abs_degrees - degrees) * 60
    minutes = int(minutes_decimal)

    seconds = (minutes_decimal - minutes) * 60

    return(sign * degrees, minutes, seconds)

class LX200Telescope:
    def __init__(self, port='/dev/ttyGEM', baudrate=9600, timeout=2):
        return

    def __enter__(self, port='/dev/ttyGEM', baudrate=9600, timeout=2):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(2)
        return self

    def calculate_checksum(self, command_id: int):
        prefix = f"{command_id}"
        b = prefix.encode('ascii')
        #print(f"prefix: {b}")
        xor = 0
        for x in b:
            xor ^= x
        chk = (xor & 0x7f) + 64
        return f'0x{chk:02X}'

    def send_command(self, command):
        full_command = command + '#'
        self.ser.write(full_command.encode('utf-8'))
        time.sleep(2)
        response = self.ser.read(50)
        response = response.decode(errors="ignore").strip('#')
        return response

    def ntp_sync(self, args):
        """ ntp sync """
        command_id = ">826:"
        checksum = self.calculate_checksum(command_id) 
        # Send a native Gemini command (><id>:<checksum>#)
        cmdstr = command_id + checksum 
        response = self.send_command(cmdstr)
        print(response)
        return

    def get_batt(self, args):
        """ get battery voltage """
        command_id = "<322:"
        checksum = self.calculate_checksum(command_id) 
        # Send a native Gemini command (<<id>:<checksum>#)
        cmdstr = command_id + checksum 
        response = self.send_command(cmdstr)
        print(f"Battery voltage: {response}")
        return

    def get_tdiff(self, args):
        """ get date """
        response = self.send_command(':GG')
        print(f"Time diff vs UTC: {response}")
        return response

    def get_echo(self, args):
        """ echo char """
        X = str(args.char)
        response = self.send_command(':CE'+X.strip('\''))
        print(response)
        return

    def get_date(self, args):
        """ get date """
        response = self.send_command(':GC')
        print(response)
        return

    def get_utc(self, args):
        """ get date """
        response = self.send_command(':Gl')
        print(response)
        return

    def get_obj(self, args):
        """ get object name """
        response = self.send_command(':Cm')
        print(response)
        return

    def goto_obj(self, args):
        """ goto selected object """
        response = self.send_command(':MS')
        print(response)
        return

    def get_ra(self, args):
        """ get right ascention """
        response = self.send_command(':P')
        if (response == "LOW  PRECISION"):
             response = self.send_command(":U") #toggle precision
        response = self.send_command(':Gr')
        print(response)
        return

    def get_dec(self, args):
        """ get declination """
        response = self.send_command(':P')
        if (response == "LOW  PRECISION"):
             response = self.send_command(":U") #toggle precision
        response = self.send_command(':Gd')
        print(response)
        return

    def get_prec(self, args):
        """ get precision """
        response = self.send_command(':P')
        print(response)
        return

    def get_lat(self, args):
        """ get site lat """ 
        response = self.send_command(':Gt')
        lat_float = float(response)
        #lat_float = (lat_float * -1)
        if response:
            print(f"LX200 site lat : {lat_float}")
            return True
        else:
            print("No response from LX200")
            return False

    def get_lon(self, args):
        """ get site lon """
        response = self.send_command(':GG')
        tdiff_float = float(response)
        response = self.send_command(':Gg')
        lon_float = float(response)
        if (tdiff_float > 0):  #gemini time offset is inverted from standard
            lon_float = (lon_float * -1)

        if response:
            print(f"LX200 site lon : {lon_float}")
            return True
        else:
            print("No response from LX200")
            return False

    def lx200(self, args):
        """ lx200 """
        response = False
        print(f"Command: {args.cmd}")
        X = str(args.cmd)
        X = X.strip('\'')
        # The first char of command must be ":", "<", or ">".
        first_char = X[0]
        if X[0] == ":":  # lx200 command
           response = self.send_command(X)
        elif X[0] == ">" or X[0] == "<":
           checksum = self.calculate_checksum(X+":")
           # Send a native Gemini command (<<id>:<checksum>#)
           cmdstr = X +":" + checksum 
           response = self.send_command(cmdstr)
        else:
           print("Command must begin with \":\", \"<\", or \">\"")

        if response:
            print(f"LX200: {response}")
            return True
        else:
            print("No response")
            return False

    def set_dbl(self, args):
        """ set double precision """
        response = self.send_command(':u')
        return

    def tgl_prec(self, args):
        """ toggle precision """
        response = self.send_command(':U')
        return

    def set_ra(self, args):
        """ set_ra HH:MM:SS """
        ra = args.ra
        ra = ra.strip('\'')

        if '-' in ra:
          sign = '-'
          ra = ra.strip('-')
        else:
          sign = '+'
          ra = ra.strip('+')

        (HH, MM, SS) = ra.split(':', 3)
        hh = int(HH)
        mm = int(MM)
        ss = float(SS) 
        if hh > 23:
          print(f"Invalid hours value: {HH}")
          return
        if mm > 59:
          print(f"Invalid minutes value: {MM}")
          return
        if ss > 59.99:
          print(f"Invalid seconds value: {SS}")
          return

        response = self.send_command(f':Sr {ra}')
        if response == '0':
          print(f"Set RA failed: {ra}")
        return

    def set_dec(self, args):
        """ set_dec +-DD:MM:SS """
        dec = args.dec
        dec = dec.strip('\'')
        if '-' in dec:
          sign = '-'
          dec = dec.strip('-')
        else:
          sign = '+'
          dec = dec.strip('+')

        (DD, MM, SS) = dec.split(':', 3)
        dd = int(DD)
        mm = int(MM)
        ss = float(SS) 
        if dd > 90:
          print(f"Invalid degree value: {DD}")
          return
        if mm > 59:
          print(f"Invalid minutes value: {MM}")
          return
        if ss > 59.99:
          print(f"Invalid seconds value: {SS}")
          return

        response = self.send_command(f':Sd {sign+dec}')
        if response == '0':
           print(f"Set DEC failed: {sign+dec}")

        return

    def set_obj(self, args):
        """ set object name """
        name = args.name
        self.send_command(':ON'+name.strip('\''))
        #print(f"Name: ON{name.strip('\'')}")
        return

    def slew_to_target(self, args):
        """ slew_to_target """
        return self.send_command(':MS')

    def quit(self, args):
        """ quit """
        return self.send_command(':Q')

    def park(self, args):
        """ park (at CWD) """
        return self.send_command(':hC')

    def park_stat(self, args):
        """ park status """
        response = self.send_command(':h?') 
        if response == '1':
            print("Telescope IS parked.")
        if response == '2':
            print("Telescope busy parking.")
        if response == '0':
            print("Telescope NOT parked.")
        return response

    def unpark(self, args):
        """ unpark (resume normal tracking) """
        return self.send_command(':hW')

    def get_rate(self, args):
        """ get rate """
        response = self.send_command(':R?') 

        if response:
            print(f"LX200: {response}")
            return True
        else:
            print("No response from LX200")
            return False

    def set_slew(self, args):
        """ get rate """
        X = str(args.slew)
        response = self.send_command(':Sw'+X.strip('\''))
        return True

    def ping(self, args):
        """ get product ID """
        response = self.send_command(':GVP')

        if response:
            print(f"LX200: {response}")
            return True
        else:
            print("No response from LX200")
            return False

    def set_lat_lon(self, args):
        # Initialize GPS receiver
        gps_receiver = GPSReceiver()
        # Get latitude and longitude from GPS
        lat, lon, utc = gps_receiver.get_location()
        if lat is None or lon is None:
            print("Could not get GPS data.")
        else:
            print(f"Latitude: {lat}, Longitude: {lon}")

            lon = (lon * -1) # Stupid Gemini !

            success_lat = 0
            success_lon = 0

            # Set the telescope location
            success_lon = self.send_command(f':Sg {lon}')
            success_lat = self.send_command(f':St {lat}')
        return

    def gps_tpv(self, args):
        # Initialize GPS receiver
        gps_receiver = GPSReceiver()
        # Get latitude and longitude from GPS
        lat, lon, utc = gps_receiver.get_location()
        if lat is None or lon is None or utc is None:
            print("Could not get GPS data.")
        else:
            print(f"Lat: {lat}, Lon: {lon}, UTC: {utc}")
        return

    def goto(self, args):
        """ goto name """
        #print(f"Name {args.name}")
        xxx = args.name.strip('\'')
        found = False
        with open('catalog', 'r') as file:
            for line in file:
                line = line.strip()
                line = line.strip('#')
                frag1, frag2, *frag3 = line.split(" ")
                if frag3:
                   frag4 = frag3.pop()
                   name, ra, dec = frag4.split(',')

                   if name == xxx:
                      found = True
                      print(f"Name {name} RA: {ra} Dec: {dec}") 

                      response = self.send_command(f':Sr {ra}')
                      if response == '0':
                          print(f"Set RA failed: {ra}")
                      self.send_command(':ON'+name)
                      response = self.send_command(f':Sd {dec}')
                      if response == '0':
                          print(f"Set DEC failed: {dec}")
                      response = self.send_command(':MS')
                      print(f"Move to object: {response} ")


        if found == False:
           print(f"Object: {args.name} not found") 

        return

    def close(self):
        self.ser.close()

    def __exit__(self, port='/dev/ttyGEM', baudrate=9600, timeout=2):
        self.ser.close()

    commands = {"unpark": unpark,
                "park": park,
                "park_stat": park_stat,
                "h?": park_stat,
                "get_rate": get_rate,
                "set_slew": set_slew,
                "ping": ping,
                "get_dec": get_dec,
                "get_ra": get_ra,
                "get_prec": get_prec,
                "tgl_prec": tgl_prec,
                "get_date": get_date,
                "get_utc": get_utc,
                "get_echo": get_echo,
                "get_lat": get_lat,
                "get_lon": get_lon,
                "set_dbl": set_dbl,
                "ntp_sync": ntp_sync,
                "get_batt": get_batt,
                "get_tdiff": get_tdiff,
                "set_obj": set_obj,
                "get_obj": get_obj,
                "set_ra": set_ra,
                "set_dec": set_dec,
                "goto_obj": goto_obj,
                "quit": quit,
                "set_lat_lon": set_lat_lon,
                "gps_tpv": gps_tpv,
                "goto": goto,
                "lx200": lx200 }

# Example of parking the telescope
if __name__ == '__main__':

    parser = argparse.ArgumentParser(description="Send a lx200 serial command")
    subparsers = parser.add_subparsers(dest="command")
    subparsers.add_parser("park_stat")
    subparsers.add_parser("unpark")
    subparsers.add_parser("park")
    subparsers.add_parser("get_rate")
    subparsers.add_parser("ping")
    subparsers.add_parser("get_prec")
    subparsers.add_parser("tgl_prec")
    subparsers.add_parser("get_date")
    subparsers.add_parser("get_utc")
    subparsers.add_parser("get_ra")
    subparsers.add_parser("get_dec")
    subparsers.add_parser("get_lat")
    subparsers.add_parser("get_lon")
    subparsers.add_parser("set_dbl")
    subparsers.add_parser("ntp_sync")
    subparsers.add_parser("get_batt")
    subparsers.add_parser("get_tdiff")
    subparsers.add_parser("get_obj")
    subparsers.add_parser("goto_obj")
    subparsers.add_parser("quit")
    subparsers.add_parser("set_lat_lon")
    subparsers.add_parser("gps_tpv")

    parser_echo = subparsers.add_parser("get_echo")
    parser_echo.add_argument('char', type=ascii)
    parser_slew = subparsers.add_parser("set_slew")
    parser_slew.add_argument('slew', type=int, choices=range(5))
    parser_obj = subparsers.add_parser("set_obj")
    parser_obj.add_argument('name', type=ascii)
    parser_ra = subparsers.add_parser("set_ra")
    parser_ra.add_argument('ra', type=ascii)
    parser_dec = subparsers.add_parser("set_dec")
    parser_dec.add_argument('dec', type=ascii)
    parser_goto = subparsers.add_parser("goto")
    parser_goto.add_argument('name', type=ascii)
    parser_lx200 = subparsers.add_parser("lx200")
    parser_lx200.add_argument('cmd', type=ascii)


    args = parser.parse_args()
    try:
        with LX200Telescope(port='/dev/ttyGEM') as scope:
           fn = scope.commands[args.command]
           fn(scope, args)
           scope.close()

    except Exception as e:
        print(f"serial error: {e}")


