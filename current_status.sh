#!/bin/bash
# look at lines in current_data.txt, remove blank character
for ZEILE in  `cat /home/pi/froeling_p3100_logger/current_data.txt | sed s/" "/""/g`
do  
  # extract the measurement number
  NR=`echo $ZEILE | cut -d ";" -f 1`  
  # extract the measturement and delete trailing ".0"
  WERT=`echo $ZEILE | cut -d ";" -f 3 | sed s/"\.0$"//g`
  EINHEIT="&deg;C" # use °C as standard unit
  INFO=""
    
  case $NR in 
      4|5|21) INFO="IST: " ;;
      29|20|6) INFO="SOLL: ";;
      8)  EINHEIT="%"
          INFO="Saugzug: ";;
      11) EINHEIT="%"
          INFO="O<sub>2</sub>: ";;
      10) EINHEIT="%"
          INFO="Einschub: ";;
      13) EINHEIT="%"
          INFO="F&uuml;llstand<br>";;
      -1|-2) EINHEIT="";;
      17) EINHEIT="%";;
      27) EINHEIT=" h" ;;
       9) EINHEIT="%"
          INFO="Zuluft: ";;
       1) EINHEIT=""
          case $WERT in
             1) WERT="abgeschaltet";;
             7) WERT="Vorbereiten";;
             2) WERT="Anheizen";;
             8) WERT="Vorw&auml;rm-<br>phase";;
             9) WERT="Z&uuml;nden";;
             3) WERT="Heizen";;
            11) WERT="Abst.Warten1";;
            13) WERT="Abst.Warten2";;
            14) WERT="Brenner aus";;
            15) WERT="Abreinigen";;
          esac ;;
      25) EINHEIT=""
          INFO="Starts: ";;
      26) EINHEIT=" h"
          INFO="Laufzeit: >";;
      #15|16) EINHEIT="&deg;C"
  esac
  
  # write output
  echo "$INFO $WERT $EINHEIT"
done
