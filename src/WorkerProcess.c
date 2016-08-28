/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int WorkerProcess( void *pv )
{
	pthread_t			timer_thread_tid ;
	
	int				nret = 0 ;
	
	signal( SIGTERM , SIG_DFL );
	signal( SIGUSR1 , SIG_IGN );
	signal( SIGUSR2 , SIG_IGN );
	
	/* 创建定时器线程，用于更新日志中的日期时间等信息 */
	nret = pthread_create( & timer_thread_tid , NULL , & TimerThread , NULL ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "pthread_create time thread failed , errno[%d]" , errno );
		return -1;
	}
	else
	{
		InfoLog( __FILE__ , __LINE__ , "parent_thread : [%lu] pthread_create TimerThread[%lu]" , (unsigned long)pthread_self() , (unsigned long)timer_thread_tid );
	}
	
	WorkerThread( pv );
	
	pthread_cancel( timer_thread_tid );
	
	return 0;
}

