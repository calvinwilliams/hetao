/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

static signed char		g_SIGUSR1_flag = 0 ;
static signed char		g_SIGUSR2_flag = 0 ;
static signed char		g_SIGTERM_flag = 0 ;
static signed char		g_exit_flag = 0 ;

static void sig_set_flag( int sig_no )
{
	UPDATEDATETIMECACHEFIRST
	InfoLog( __FILE__ , __LINE__ , "recv signal[%d]" , sig_no );
	
	/* 信号回调函数，只是设置标志变量 */
	if( sig_no == SIGUSR1 )
	{
		g_SIGUSR1_flag = 1 ;
	}
	else if( sig_no == SIGUSR2 )
	{
		g_SIGUSR2_flag = 1 ;
	}
	else if( sig_no == SIGTERM )
	{
		g_SIGTERM_flag = 1 ;
	}
	
	return;
}

static void sig_proc( struct HetaoServer *p_server )
{
	int		i ;
	
	int		nret = 0 ;
	
	UPDATEDATETIMECACHEFIRST
	
	/* 处理标志变量 */
	if( g_SIGUSR1_flag == 1 )
	{
		/* 通知所有工作进程重新打开事件日志 */
		for( i = 0 ; i < g_p_server->p_config->worker_processes ; i++ )
		{
			char	ch = SIGNAL_REOPEN_LOG ;
			nret = write( g_p_server->process_info_array[i].pipe[1] , & ch , 1 ) ;
		}
		
		g_SIGUSR1_flag = 0 ;
	}
	else if( g_SIGUSR2_flag == 1 )
	{
		pid_t			pid ;
		
		int			nret = 0 ;
		
		/* 保存侦听信息到环境变量中 */
		nret = SaveListenSockets( g_p_server ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "SaveListenSockets faild[%d]" , nret );
			return;
		}
		
		/* 产生下一辈进程 */
		pid = fork() ;
		if( pid == -1 )
		{
			;
		}
		else if( pid == 0 )
		{
			InfoLog( __FILE__ , __LINE__ , "execvp ..." );
			execvp( "hetao" , g_p_server->argv );
			FatalLog( __FILE__ , __LINE__ , "execvp failed , errno[%d]" , errno );
			
			exit(9);
		}
		
		g_SIGUSR2_flag = 0 ;
	}
	else if( g_SIGTERM_flag == 1 )
	{
		/* 以关闭通知管道的方式通知所有工作进程优雅结束 */
		for( i = 0 ; i < g_p_server->p_config->worker_processes ; i++ )
		{
			DebugLog( __FILE__ , __LINE__ , "Close pipe[%d]" , g_p_server->process_info_array[i].pipe[1] );
			close( g_p_server->process_info_array[i].pipe[1] );
		}
		
		g_SIGTERM_flag = 0 ;
		g_exit_flag = 1 ;
	}
	
	return;
}

int MonitorProcess( void *pv )
{
	struct HetaoServer	*p_server = (struct HetaoServer *)pv ;
	
	int			i , j ;
	int			worker_processes ;
	
	struct sigaction	act ;
	
	pid_t			pid ;
	int			status ;
	
	int			nret = 0 ;
	
	SETPID
	SETTID
	UPDATEDATETIMECACHEFIRST
	InfoLog( __FILE__ , __LINE__ , "--- master begin ---" );
	
	/* 只响应信号TERM、USR1、USR2 */
	/*
		TERM : 结束
		USR1 : 重新打开事件日志
		USR2 : 产生下一辈进程，用于优雅重启
	*/
	act.sa_handler = & sig_set_flag ;
	sigemptyset( & (act.sa_mask) );
	act.sa_flags = 0 ;
	signal( SIGCLD , SIG_DFL );
	signal( SIGCHLD , SIG_DFL );
	signal( SIGPIPE , SIG_IGN );
	sigaction( SIGTERM , & act , NULL );
	sigaction( SIGUSR1 , & act , NULL );
	sigaction( SIGUSR2 , & act , NULL );
	
	/* 创建所有工作进程组 */
	for( i = 0 ; i < p_server->p_config->worker_processes ; i++ )
	{
		/* 创建命令管道 */
		nret = pipe( p_server->process_info_array[i].pipe ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "pipe failed , errno[%d]" , errno );
			return -1;
		}
		SetHttpCloseExec( p_server->process_info_array[i].pipe[0] );
		SetHttpCloseExec( p_server->process_info_array[i].pipe[1] );
		DebugLog( __FILE__ , __LINE__ , "Create pipe #%d# #%d#" , p_server->process_info_array[i].pipe[0] , p_server->process_info_array[i].pipe[1] );
		
		/* 创建工作进程 */
		p_server->p_this_process_info = p_server->process_info_array + i ;
		p_server->process_info_index = i ;
		
		pid = fork() ;
		UPDATEDATETIMECACHEFIRST
		if( pid == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "fork failed , errno[%d]" , errno );
			return -1;
		}
		else if( pid == 0 )
		{
			SETPID
			SETTID
			UPDATEDATETIMECACHE
			
			InfoLog( __FILE__ , __LINE__ , "child : [%ld] fork [%ld]" , getppid() , getpid() );
			
			close( p_server->process_info_array[i].pipe[1] ) ;
			DebugLog( __FILE__ , __LINE__ , "pipe #%d# close #%d#" , p_server->process_info_array[i].pipe[0] , p_server->process_info_array[i].pipe[1] );
			for( j = i - 1 ; j >= 0 ; j-- )
			{
				close( p_server->process_info_array[j].pipe[1] ) ;
				DebugLog( __FILE__ , __LINE__ , "pipe close #%d#" , p_server->process_info_array[j].pipe[1] );
			}
			
			nret = WorkerProcess((void*)p_server) ;
			
			CleanServerEnvirment( p_server );
			
			return -nret;
		}
		else
		{
			p_server->process_info_array[i].pid = pid ;
			
			InfoLog( __FILE__ , __LINE__ , "parent : [%ld] fork [%ld]" , getpid() , p_server->process_info_array[i].pid );
			close( p_server->process_info_array[i].pipe[0] ) ;
			DebugLog( __FILE__ , __LINE__ , "pipe #%d# close #%d#" , p_server->process_info_array[i].pipe[1] , p_server->process_info_array[i].pipe[0] );
		}
	}
	
	/* 监控所有工作进程。如果发现有结束，重启它 */
	worker_processes = p_server->p_config->worker_processes ;
	while( worker_processes > 0 )
	{
_WAITPID :
		pid = waitpid( -1 , & status , 0 );
		UPDATEDATETIMECACHEFIRST
		if( pid == -1 )
		{
			if( errno == EINTR )
			{
				sig_proc( p_server );
				goto _WAITPID;
			}
			
			ErrorLog( __FILE__ , __LINE__ , "waitpid failed , errno[%d]" , errno );
			return -1;
		}
		
		if( WEXITSTATUS(status) == 0 && WIFSIGNALED(status) == 0 && WTERMSIG(status) == 0 )
		{
			InfoLog( __FILE__ , __LINE__
				, "waitpid[%ld] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d]"
				, pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) );
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__
				, "waitpid[%ld] WEXITSTATUS[%d] WIFSIGNALED[%d] WTERMSIG[%d]"
				, pid , WEXITSTATUS(status) , WIFSIGNALED(status) , WTERMSIG(status) );
		}
		
		for( i = 0 ; i < p_server->p_config->worker_processes ; i++ )
		{
			if( p_server->process_info_array[i].pid == pid )
				break;
		}
		if( i >= p_server->p_config->worker_processes )
			goto _WAITPID;
		
		close( p_server->process_info_array[i].pipe[1] );
		
		worker_processes--;
		InfoLog( __FILE__ , __LINE__ , "worker_processes[%d]" , worker_processes );
		
		if( g_exit_flag == 1 )
			continue;
		
		sleep(1);
		
		/* 创建管道 */
		nret = pipe( p_server->process_info_array[i].pipe ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "pipe failed , errno[%d]" , errno );
			return -1;
		}
		SetHttpCloseExec( p_server->process_info_array[i].pipe[0] );
		SetHttpCloseExec( p_server->process_info_array[i].pipe[1] );
		DebugLog( __FILE__ , __LINE__ , "Create pipe #%d# #%d#" , p_server->process_info_array[i].pipe[0] , p_server->process_info_array[i].pipe[1] );
		
		/* 创建工作进程 */
		p_server->p_this_process_info = p_server->process_info_array + i ;
		p_server->process_info_index = i ;
		
_FORK :
		p_server->process_info_array[i].pid = fork() ;
		UPDATEDATETIMECACHEFIRST
		if( p_server->process_info_array[i].pid == -1 )
		{
			if( errno == EINTR )
			{
				sig_proc( p_server );
				goto _FORK;
			}
			
			ErrorLog( __FILE__ , __LINE__ , "fork failed , errno[%d]" , errno );
			return -1;
		}
		else if( p_server->process_info_array[i].pid == 0 )
		{
			SETPID
			SETTID
			UPDATEDATETIMECACHE
			
			InfoLog( __FILE__ , __LINE__ , "child : [%ld] fork [%ld]" , getppid() , getpid() );
			
			close( p_server->process_info_array[i].pipe[1] ) ;
			DebugLog( __FILE__ , __LINE__ , "pipe #%d# close #%d#" , p_server->process_info_array[i].pipe[0] , p_server->process_info_array[i].pipe[1] );
			for( j = p_server->p_config->worker_processes-1 ; j >= 0 ; j-- )
			{
				if( j != i )
				{
					close( p_server->process_info_array[j].pipe[1] ) ;
					DebugLog( __FILE__ , __LINE__ , "pipe close #%d#" , p_server->process_info_array[j].pipe[1] );
				}
			}
			
			nret = WorkerProcess((void*)p_server) ;
			
			CleanServerEnvirment( p_server );
			
			return -nret;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "parent : [%ld] fork [%ld]" , getpid() , p_server->process_info_array[i].pid );
			close( p_server->process_info_array[i].pipe[0] ) ;
			DebugLog( __FILE__ , __LINE__ , "pipe #%d# close #%d#" , p_server->process_info_array[i].pipe[1] , p_server->process_info_array[i].pipe[0] );
		}
		
		worker_processes++;
		InfoLog( __FILE__ , __LINE__ , "worker_processes[%d]" , worker_processes );
	}
	
	CleanServerEnvirment( p_server );
	
	InfoLog( __FILE__ , __LINE__ , "--- master exit ---" );
	
	return 0;
}

