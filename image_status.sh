#1/bin/bash
# this script is called by motion when a new image was saved
# it connects a specific image with a status code to debug the various status codes

image=$1;
for ZEILE in  `cat /home/pi/froeling_p3100_logger/current_data.txt | sed s/" "/""/g`
  do
  # extrahiert die Messwertnummer
  NR=`echo $ZEILE | cut -d ";" -f 1`
  # extrahiert den Messwert und loescht "" am Ende
  WERT=`echo $ZEILE | cut -d ";" -f 3 | sed s/"\$"//g`
  INFO=""

  case $NR in
      -2) INFO="Anlagendatum"
          DATE="$WERT"
      ;;
      -1) INFO="Anlagenzeit"
          TIME="$WERT"
       ;;
       0) INFO="Zustand 0"
                  if [ "$WERT" != "$STATUS0" ]
                  then
                      STATUS0="$WERT"
                  fi
       ;;
       1) INFO="Zustand 1"
                  if [ "$WERT" != "$STATUS1" ]
                  then
                                   STATUS1="$WERT"
                  fi
       ;;
       2) INFO="Zustand 2"
                  if [ "$WERT" != "$STATUS2" ]
                  then
                                   STATUS2="$WERT"
                  fi
       ;;
esac
done

cat /home/pi/froeling_p3100_logger/current_data.txt | mail -s "WOHNRAUM - Image - Status: $image: $STATUS0-$STATUS1-$STATUS2" $EMAIL



