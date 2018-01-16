#/bin/bash
curl -T /home/pi/froeling_p3100_logger/current_data.txt -u $FTP_USER:$FTP_PASSWORD $FTP_HOST 
 
