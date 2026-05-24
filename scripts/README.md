The lx200_cmd.py file is a python script that uses the serial interface on RPi attached to the gemini to execute the varioue lx200 commands.

        Ex: ./lx200_cmd.py lx200 <322      #get_batt
        Ex: ./lx200_cmd.py get_batt
        Ex: ./lx200_cmd.py get_echo A
        Ex: ./lx200_cmd.py set_lat_lon     # get the site lat and lon from the gps and set them to gemini 
        Ex: ./lx200_cmd.py ntp_sync        # sync the gemini clock to the RPi chrony NTP. 
