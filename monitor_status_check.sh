#!/bin/sh
# this script checks if the check_status.sh script is running, if not, it will restart it, and send a notification message
if ps -ef | fgrep -v grep | fgrep check_status.sh ; then
        echo "script is running"
        exit 0
else
        echo "script is not running - restarting"
        /home/pi/froeling_p3100_logger/check_status.sh >> /home/pi/froeling_p3100_logger/check_status.log &
        #mailing program
        cat /home/pi/froeling_p3100_logger/current_data.txt | mail -s "WOHNRAUM - Heizung: check-status was not running - restarted!" $EMAIL 
        exit 0
fi
