if [ $# -ne 5 ] ; then
	echo "USAGE : press.sh C_PARAM N_PARAM STYLE ROUNDS PRESS_FILE"
	exit 9
fi

C_PARAM=$1
N_PARAM=$2
STYLE=$3
MAX_ROUND=$4
PRESS_FILE=$5

ROUND=0
while [ $ROUND -le $MAX_ROUND ] ; do
	if [ x"$STYLE" = x"1" ] ; then
		TIME1=`ab -c $C_PARAM -n $N_PARAM http://192.168.6.111:9527/$PRESS_FILE | grep "Requests per second" | awk '{print $4}'`
	elif [ x"$STYLE" = x"2" ] ; then
		TIME1=`ab -kc $C_PARAM -n $N_PARAM http://192.168.6.111:9527/$PRESS_FILE | grep 'Requests per second' | awk '{print $4}'`
	elif [ x"$STYLE" = x"3" ] ; then
		TIME1=`ab2 -kc $C_PARAM -n $N_PARAM -H "Accept-Encoding: gzip" http://192.168.6.111:9527/$PRESS_FILE | grep 'Requests per second' | awk '{print $4}'`
	fi
	
	sleep 1
	
	if [ x"$STYLE" = x"1" ] ; then
		TIME2=`ab -c $C_PARAM -n $N_PARAM http://192.168.6.111:9528/$PRESS_FILE | grep "Requests per second" | awk '{print $4}'`
	elif [ x"$STYLE" = x"2" ] ; then
		TIME2=`ab -kc $C_PARAM -n $N_PARAM http://192.168.6.111:9528/$PRESS_FILE | grep 'Requests per second' | awk '{print $4}'`
	elif [ x"$STYLE" = x"3" ] ; then
		TIME2=`ab2 -kc $C_PARAM -n $N_PARAM -H "Accept-Encoding: gzip" http://192.168.6.111:9528/$PRESS_FILE | grep 'Requests per second' | awk '{print $4}'`
	fi
	
	sleep 120
	
	if [ x"$STYLE" = x"1" ] ; then
		TIME3=`ab -c $C_PARAM -n $N_PARAM http://192.168.6.111:9529/$PRESS_FILE | grep "Requests per second" | awk '{print $4}'`
	elif [ x"$STYLE" = x"2" ] ; then
		TIME3=`ab -kc $C_PARAM -n $N_PARAM http://192.168.6.111:9529/$PRESS_FILE | grep 'Requests per second' | awk '{print $4}'`
	elif [ x"$STYLE" = x"3" ] ; then
		TIME3=`ab2 -kc $C_PARAM -n $N_PARAM -H "Accept-Encoding: gzip" http://192.168.6.111:9529/$PRESS_FILE | grep 'Requests per second' | awk '{print $4}'`
	fi
	
	sleep 1
	
	if [ x"$STYLE" = x"1" ] ; then
		TIME4=`ab -c $C_PARAM -n $N_PARAM http://192.168.6.111:9530/$PRESS_FILE | grep "Requests per second" | awk '{print $4}'`
	elif [ x"$STYLE" = x"2" ] ; then
		TIME4=`ab -kc $C_PARAM -n $N_PARAM http://192.168.6.111:9530/$PRESS_FILE | grep 'Requests per second' | awk '{print $4}'`
	elif [ x"$STYLE" = x"3" ] ; then
		TIME4=`ab2 -kc $C_PARAM -n $N_PARAM -H "Accept-Encoding: gzip" http://192.168.6.111:9530/$PRESS_FILE | grep 'Requests per second' | awk '{print $4}'`
	fi
	
	sleep 1
	
	if [ x"$TIME1" = x"" ] ; then
		TIMES1[$ROUND]=0
	else
		TIMES1[$ROUND]=$TIME1
	fi
	if [ x"$TIME2" = x"" ] ; then
		TIMES2[$ROUND]=0
	else
		TIMES2[$ROUND]=$TIME2
	fi
	if [ x"$TIME3" = x"" ] ; then
		TIMES3[$ROUND]=0
	else
		TIMES3[$ROUND]=$TIME3
	fi
	if [ x"$TIME4" = x"" ] ; then
		TIMES4[$ROUND]=0
	else
		TIMES4[$ROUND]=$TIME4
	fi
	
	echo "ROUND: $ROUND    hetao: ${TIMES1[$ROUND]}    nginx: ${TIMES2[$ROUND]}    apache: ${TIMES3[$ROUND]}    tengine: ${TIMES4[$ROUND]}"
	sleep 1
	ROUND=`expr $ROUND + 1`
done

echo "--- result ---------"

ROUND=0
echo "ROUND    hetao        nginx          apache          tengine"
while [ $ROUND -le $MAX_ROUND ] ; do
	echo "$ROUND     ${TIMES1[$ROUND]}     ${TIMES2[$ROUND]}      ${TIMES3[$ROUND]}      ${TIMES4[$ROUND]}"
	ROUND=`expr $ROUND + 1`
done

