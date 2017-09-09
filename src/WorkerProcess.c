/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

extern signed char			g_worker_exit_flag ;

#if ( defined _WIN32 )
DWORD WINAPI WaitForParentProcessThread( LPVOID lpParameter )
{
	struct HetaoEnv	*p_env = (struct HetaoEnv *)lpParameter ;
	DWORD		dwret ;
	
	p_env->p_process_info->hParentProcess = OpenProcess( PROCESS_VM_OPERATION|SYNCHRONIZE , FALSE , p_env->p_process_info->dwParentProcessId ) ;
	dwret = WaitForSingleObject( p_env->p_process_info->hParentProcess , INFINITE ) ;
	
	g_worker_exit_flag = 1 ;
	
	return 0;
}
#endif

int WorkerProcess( void *pv )
{
	struct HetaoEnv			*p_env = (struct HetaoEnv *)pv ;
	
#if ( defined __linux ) || ( defined __unix )
	pthread_t			timer_thread_tid ;
#elif ( defined _WIN32 )
	HANDLE				timer_thread_tid ;
	HANDLE				waitforparentprocess_thread_tid ;
#endif
	
	int				nret = 0 ;
	
	if( p_env->pwd )
	{
#if ( defined __linux ) || ( defined __unix )
		setuid( p_env->pwd->pw_uid );
		InfoLog( __FILE__ , __LINE__ , "setuid[%d]" , p_env->pwd->pw_uid );
		setgid( p_env->pwd->pw_gid );
		InfoLog( __FILE__ , __LINE__ , "setgid[%d]" , p_env->pwd->pw_gid );
#elif ( defined _WIN32 )
#endif
	}
	
#if ( defined __linux ) || ( defined __unix )
	signal( SIGTERM , SIG_DFL );
	signal( SIGUSR1 , SIG_IGN );
	signal( SIGUSR2 , SIG_IGN );
#elif ( defined _WIN32 )
#endif
	
#if ( defined __linux ) || ( defined __unix )
	/* 创建定时器线程，用于更新日志中的日期时间等信息 */
	nret = pthread_create( & timer_thread_tid , NULL , & TimerThread , NULL ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "pthread_create time thread failed , errno[%d]" , ERRNO );
		return -1;
	}
	else
	{
		InfoLog( __FILE__ , __LINE__ , "parent_thread : [%lu] pthread_create TimerThread[%lu]" , (unsigned long)g_tid , (unsigned long)timer_thread_tid );
	}
#elif ( defined _WIN32 )
	/* 创建定时器线程，用于更新日志中的日期时间等信息 */
	timer_thread_tid = CreateThread( NULL , 0 , & TimerThread , NULL , 0 , NULL ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "pthread_create time thread failed , errno[%d]" , ERRNO );
		return -1;
	}
	else
	{
		InfoLog( __FILE__ , __LINE__ , "parent_thread : [%lu] pthread_create TimerThread[%lu]" , (unsigned long)g_tid , (unsigned long)timer_thread_tid );
	}
	
	if( p_env->p_process_info->pi.hProcess )
	{
		waitforparentprocess_thread_tid = CreateThread( NULL , 0 , & WaitForParentProcessThread , pv , 0 , NULL ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "pthread_create waitforparentprocess thread failed , errno[%d]" , ERRNO );
			return -1;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "parent_thread : [%lu] pthread_create WaitForParentProcessThread[%lu]" , (unsigned long)g_tid , (unsigned long)waitforparentprocess_thread_tid );
		}
	}
#endif
	
	WorkerThread( pv );
	
#if ( defined _WIN32 )
	TerminateThread( timer_thread_tid , 0 );
	if( p_env->p_process_info->pi.hProcess )
	{
		TerminateThread( waitforparentprocess_thread_tid , 0 );
	}
#endif
	
	CleanEnvirment( p_env );
	
	return 0;
}

