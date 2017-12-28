#!/bin/bash -x
# sleep 60

##############################################################################
#
#  kuemmert sich darum, dass froeling_p3100_logger laeuft oder laufen soll/darf
#  wird in regelmaessigen Anstaenden vom crond ausgefuehrt
#
##############################################################################

WORKDIR="/home/pi/froeling_p3100_logger"

ACTUAL="./current_data.txt" # Datei, in die regelmaessig Messwert geschrieben werden
ACTUALINTERVALL=5
LOGINTERVALL=60
COMPORT=3
INITDAUER=120

WATCHDOGNAME=watchdog.sh
LOGGERNAME=froeling_p3100_logger
LOGGERCOMMAND="nice -n +15 ./$LOGGERNAME $COMPORT $LOGINTERVALL $ACTUALINTERVALL 0"

##############################################################################

function stoppeAlteProzesse {
  # stoppt alle watchdog.sh-Skripte, die älter als 1min sind
  killall -o 1m -9 watchdog.sh
}

function laeuftLogger {
  # ueberprueft, ob $ACTUAL juenger als 1 Minute ist
  test `find $ACTUAL -cmin 1 | wc -l` -ge 1 
}

function stoppeLogger {
  killall -KILL -r $LOGGERNAME
}

function starteLogger {
  # stoppt evtl. laufendes Loggerprogramm und startet es dann neu
  stoppeLogger
  $LOGGERCOMMAND &
  sleep $INITDAUER

  # nach einer Wartezeit von $INITDAUER sek wird überprüft, ob Looger läuft
  # wenn nein, wird erneut gestartet
  while ! laeuftLogger
  do
    stoppeLogger
    sleep 1
    $LOGGERCOMMAND &
    sleep $INITDAUER
  done
}

##############################################################################
#
# Hauptprogramm
#
##############################################################################

# nur ein Debugeintrag, kann auch weg
#echo `date` watchdog.sh wird ausführt. >> /root/watchdog.log

# beendet alle watchdog.sh Prozesse
stoppeAlteProzesse 

# wechselt ins Arbeitsverzeichnis, damit relative Pfade von $LOGGERNAME passen
cd $WORKDIR

# ueberprueft ob Logger laufen soll und startet bzw. stoppt ggf.
if laeuftLogger
     then echo Logger läuft, alles o.k.  > /dev/null
     else starteLogger
fi
