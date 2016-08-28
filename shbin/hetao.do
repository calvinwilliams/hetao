#!/bin/bash

usage()
{
	echo "USAGE : hetao.do [ status | start | stop | kill | restart | restart_graceful | relog ]"
}

if [ $# -eq 0 ] ; then
	usage
	exit 9
fi

case $1 in
	status)
		ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk '{if($3=="1")print $0}'
		ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk '{if($3!="1")print $0}'
		;;
	start)
		PID=`ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk '{if($3=="1")print $2}'`
		if [ x"$PID" != x"" ] ; then
			echo "*** WARN : hetao existed"
			exit 1
		fi
		hetao ~/etc/hetao.conf
		if [ $? -ne 0 ] ; then
			exit 1
		fi
		while [ 1 ] ; do
			sleep 1
			PID=`ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk '{if($3=="1")print $2}'`
			if [ x"$PID" != x"" ] ; then
				break
			fi
		done
		echo "hetao start ok"
		hetao.do status
		;;
	stop)
		hetao.do status
		if [ $? -ne 0 ] ; then
			exit 1
		fi
		PID=`ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk '{if($3=="1")print $2}'`
		if [ x"$PID" = x"" ] ; then
			echo "*** WARN : hetao not existed"
			exit 1
		fi
		kill $PID
		while [ 1 ] ; do
			sleep 1
			PID=`ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk '{if($3=="1")print $2}'`
			if [ x"$PID" = x"" ] ; then
				break
			fi
		done
		echo "hetao end ok"
		;;
	kill)
		hetao.do status
		killall -9 hetao
		;;
	restart)
		hetao.do stop
		hetao.do start
		;;
	restart_graceful)
		hetao.do status
		if [ $? -ne 0 ] ; then
			exit 1
		fi
		PID=`ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk '{if($3=="1")print $2}'`
		if [ x"$PID" = x"" ] ; then
			echo "*** WARN : hetao not existed"
			exit 1
		fi
		kill -USR2 $PID
		while [ 1 ] ; do
			sleep 1
			PID2=`ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk -v pid="$PID" '{if($3=="1"&&$2!=pid)print $2}'`
			if [ x"$PID2" != x"" ] ; then
				break
			fi
		done
		echo "new hetao pid[$PID2] start ok"
		kill $PID
		while [ 1 ] ; do
			sleep 1
			PID3=`ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk -v pid="$PID" '{if($3=="1"&&$2==pid)print $2}'`
			if [ x"$PID3" = x"" ] ; then
				break
			fi
		done
		echo "old hetao pid[$PID] end ok"
		hetao.do status
		if [ $? -ne 0 ] ; then
			exit 1
		fi
		;;
	relog)
		hetao.do status
		if [ $? -ne 0 ] ; then
			exit 1
		fi
		PID=`ps -ef | grep -w hetao | grep -v "hetao.do" | grep -v grep | awk '{if($3=="1")print $2}'`
		if [ x"$PID" = x"" ] ; then
			echo "*** WARN : hetao not existed"
			exit 1
		fi
		kill -USR1 $PID
		echo "send signal to hetao for reopenning log"
		;;
	*)
		usage
		;;
esac

