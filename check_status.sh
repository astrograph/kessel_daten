#!/bin/bash
# Skript soll kontinuierlich laufen, checkt staendig den Status
# wird ueber ein anderes Script, das per crontab gestartet wird ueberwacht
# sehe zeilenweise in actual_data.txt nach und loesche Leerzeichen
# Umgebungsvariablen sind in /etc/environment gesetzt
BETRIEBSZUSTAND="x";
BETRIEBSART="x";
FEHLERSTATUS="x";
BETRIEBSZUSTAND_TEXT="x";
BETRIEBSART_TEXT="x";
FEHLERSTATUS_TEXT="x";
STATUS0="x";
STATUS1="x";
STATUS2="x";
COMPOUND_STATUS="";
DATE="";
TIME="";

while :
do
  for ZEILE in  `cat /home/pi/froeling_p3100_logger/current_data.txt | sed s/" "/""/g`
  do  
    # extrahiert die Messwertnummer 
    NR=`echo $ZEILE | cut -d ";" -f 1`  
    # extrahiert den Messwert und loescht "" am Ende
    WERT=`echo $ZEILE | cut -d ";" -f 3 | sed s/"\$"//g`
    INFO=""
    case $NR in 
      -2) INFO="Anlagendatum"
         DATE="$WERT";;
      -1) INFO="Anlagenzeit"
         TIME="$WERT" ;;
       0) INFO="Zustand 0"
         if [ "$WERT" != "$STATUS0" ]; then 
           STATUS0="$WERT";
           BETRIEBSART="$WERT";
         fi;;
       1) INFO="Zustand 1"
         if [ "$WERT" != "$STATUS1" ]; then
           STATUS1="$WERT"
           BETRIEBSZUSTAND="$WERT"
         fi ;;
       2) INFO="Zustand 2"
         if [ "$WERT" != "$STATUS2" ]; then
           STATUS2="$WERT"
           FEHLERSTATUS="$WERT"
         fi ;;  
    esac
    # echo "$INFO $WERT $STATUS0 $STATUS1 $STATUS2"   
  done

  if [ "$BETRIEBSART" == "0.0" ]; then
    BETRIEBSART_TEXT="Winterbetrieb";
  elif [ "$BETRIEBSART" == "1.0" ]; then
    BETRIEBSART_TEXT="Sommerbetrieb (?)";
  elif [ "$BETRIEBSART" == "2.0" ]; then
    BETRIEBSART_TEXT="Uebergangsbetrieb";
  elif [ "$BETRIEBSART" == "3.0" ]; then
    BETRIEBSART_TEXT="Scheitholzbetrieb (?)";
  elif [ "$BETRIEBSART" == "4.0" ]; then
    BETRIEBSART_TEXT="Reinigen (?)";
  elif [ "$BETRIEBSART" == "5.0" ]; then
    BETRIEBSART_TEXT="Kaminfeger (?)";
  elif [ "$BETRIEBSART" == "6.0" ]; then
    BETRIEBSART_TEXT="Ausgeschaltet";
  fi 

  if [ "$BETRIEBSZUSTAND" == "0.0" ]; then
    BETRIEBSZUSTAND_TEXT="Stoerung";
  elif [ "$BETRIEBSZUSTAND" == "1.0" ]; then
    BETRIEBSZUSTAND_TEXT="Brenner aus";
  elif [ "$BETRIEBSZUSTAND" == "2.0" ]; then
    BETRIEBSZUSTAND_TEXT="Anheizen";
  elif [ "$BETRIEBSZUSTAND" == "3.0" ]; then
    BETRIEBSZUSTAND_TEXT="Heizen";
  elif [ "$BETRIEBSZUSTAND" == "4.0" ]; then
    BETRIEBSZUSTAND_TEXT="Feuerhaltung";
  elif [ "$BETRIEBSZUSTAND" == "5.0" ]; then
    BETRIEBSZUSTAND_TEXT="Feuer aus";
  elif [ "$BETRIEBSZUSTAND" == "6.0" ]; then
    BETRIEBSZUSTAND_TEXT="Tuer offen";
  elif [ "$BETRIEBSZUSTAND" == "7.0" ]; then
    BETRIEBSZUSTAND_TEXT="Vorbereitung";
  elif [ "$BETRIEBSZUSTAND" == "8.0" ]; then
    BETRIEBSZUSTAND_TEXT="Vorwaermphase";
  elif [ "$BETRIEBSZUSTAND" == "9.0" ]; then
    BETRIEBSZUSTAND_TEXT="Zuenden";
  elif [ "$BETRIEBSZUSTAND" == "10.0" ]; then
    BETRIEBSZUSTAND_TEXT="unknown";
  elif [ "$BETRIEBSZUSTAND" == "11.0" ]; then
    BETRIEBSZUSTAND_TEXT="Abst. warten 1";
  elif [ "$BETRIEBSZUSTAND" == "12.0" ]; then
    BETRIEBSZUSTAND_TEXT="Abst. Einschub 1";
  elif [ "$BETRIEBSZUSTAND" == "13.0" ]; then
    BETRIEBSZUSTAND_TEXT="Abst. Warten 2";
  elif [ "$BETRIEBSZUSTAND" == "14.0" ]; then
    BETRIEBSZUSTAND_TEXT="Abst. Einschub 2";
  elif [ "$BETRIEBSZUSTAND" == "15.0" ]; then
    BETRIEBSZUSTAND_TEXT="Abreinigen";
  fi

  STATE_STRING="$BETRIEBSART_TEXT - $BETRIEBSZUSTAND_TEXT"
  #echo "Status: $STATUS0-$STATUS1-$STATUS2"
  NEW_COMPOUND="$BETRIEBSZUSTAND-$FEHLERSTATUS";
  #echo "COMPOUND_STATUS: $COMPOUND_STATUS"  
  #echo "NEW_COMPOUND: $NEW_COMPOUND"
if [ "$NEW_COMPOUND" != "$COMPOUND_STATUS" ]
  then
    # new state -> do something!
    COMPOUND_STATUS="$NEW_COMPOUND"
      if [ "$COMPOUND_STATUS" == "11.0-7.0" ]; then
          STATE_STRING="$BETRIEBSART_TEXT - Fehler: Isoliertuer offen - Aschebox voll oder Lagerraum kontrollieren!"
	  cat /home/pi/froeling_p3100_logger/current_data.txt | mail -s "WOHNRAUM - Heizung: $STATE_STRING" $EMAIL_ALARM
      elif [ "$COMPOUND_STATUS" == "3.0-1.0" ]; then
	  cat /home/pi/froeling_p3100_logger/current_data.txt | mail -s "WOHNRAUM - Heizung: $STATE_STRING" $EMAIL
      elif [ "$BETRIEBSZUSTAND == "0.0" ]; then
	  cat /home/pi/froeling_p3100_logger/current_data.txt | mail -s "WOHNRAUM - Heizung: $STATE_STRING" $EMAIL
      elif [ "$BETRIEBSZUSTAND == "6.0" ]; then
	  cat /home/pi/froeling_p3100_logger/current_data.txt | mail -s "WOHNRAUM - Heizung: $STATE_STRING" $EMAIL
      fi
      NOW=`date "+%Y-%m-%d %H:%M:%S"`     	  
      echo "$NOW;$DATE;$TIME;$BETRIEBSART-$COMPOUND_STATUS;$STATE_STRING"
  fi
  sleep 2;
done
#


