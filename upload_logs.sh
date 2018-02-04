#/bin/bash
curl -T /home/pi/froeling_p3100_logger/logfiles/froeling_data_`date +%Y-%m-%d`.log -u FTP_USER:FTP_PASSWORD FTP_HOST
