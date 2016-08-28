/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

void *TimerThread( void *pv )
{
	/* 每隔一秒钟，更新写日志的日期时间缓冲区、秒漏事件 */
	while(1)
	{
		UPDATEDATETIMECACHE
		
		sleep(1);
		
		g_second_elapse = 1 ;
	}
	
	pthread_exit(NULL);
}

