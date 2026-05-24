The lx200_cmd.py file is a python script that uses the serial interface on RPi attached to the gemini to execute the varioue lx200 commands.

        Ex: ./lx200_cmd.py lx200 <322      # genaric cmd handler. First char must ":", "<", or ">". Depending on command set.
        Ex: ./lx200_cmd.py get_batt        # human friendly version of "./lx200_cmd.py lx200 <322"
        Ex: ./lx200_cmd.py get_echo A      # tell gemini to echo a char
        Ex: ./lx200_cmd.py set_lat_lon     # get the site lat and lon from the gps and send them to gemini 
        Ex: ./lx200_cmd.py ntp_sync        # sync the gemini clock to the RPi chrony NTP. 

The bash script "startup" is example of how to sync the gemini after a cold start, assuming a moble setup.

