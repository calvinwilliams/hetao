/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

static void usage()
{
	printf( "hetao v%s build %s %s\n" , __HETAO_VERSION , __DATE__ , __TIME__ );
	printf( "USAGE : hetao hetao.conf\n" );
	return;
}

int main( int argc , char *argv[] )
{
	hetao_conf		*p_conf = NULL ;
	struct HetaoEnv		*p_env = NULL ;
	
	int			nret = 0 ;
	
	umask(0);
	
	if( argc == 1 + 1 )
	{
		/* 申请环境结构内存 */
		p_env = (struct HetaoEnv *)malloc( sizeof(struct HetaoEnv) ) ;
		if( p_env == NULL )
		{
			if( getenv( HETAO_LISTEN_SOCKFDS ) == NULL )
				printf( "alloc failed[%d] , errno[%d]\n" , nret , errno );
			return 1;
		}
		memset( p_env , 0x00 , sizeof(struct HetaoEnv) );
		g_p_env = p_env ;
		p_env->argv = argv ;
		
		/* 申请配置结构内存 */
		p_conf = (hetao_conf *)malloc( sizeof(hetao_conf) ) ;
		if( p_conf == NULL )
		{
			if( getenv( HETAO_LISTEN_SOCKFDS ) == NULL )
				printf( "alloc failed[%d] , errno[%d]\n" , nret , errno );
			return 1;
		}
		memset( p_conf , 0x00 , sizeof(hetao_conf) );
		
		/* 重置所有HTTP状态码、描述为缺省 */
		ResetAllHttpStatus();
		
		/* 设置缺省主日志 */
		if( getenv( HETAO_LOG_PATHFILENAME ) == NULL )
			SetLogFile( "#" );
		else
			SetLogFile( getenv(HETAO_LOG_PATHFILENAME) );
		SetLogLevel( LOGLEVEL_ERROR );
		SETPID
		SETTID
		UPDATEDATETIMECACHEFIRST
		
		/* 装载配置 */
		strncpy( p_env->config_pathfilename , argv[1] , sizeof(p_env->config_pathfilename)-1 );
		nret = LoadConfig( p_env->config_pathfilename , p_conf , p_env ) ;
		if( nret )
		{
			if( getenv( HETAO_LISTEN_SOCKFDS ) == NULL )
				printf( "LoadConfig failed[%d]\n" , nret );
			free( p_env );
			return -nret;
		}
		
		/* 重新设置主日志 */
		CloseLogFile();
		
		SetLogFile( p_conf->error_log );
		SetLogLevel( p_env->log_level );
		SETPID
		SETTID
		UPDATEDATETIMECACHEFIRST
		InfoLog( __FILE__ , __LINE__ , "--- hetao v%s build %s %s ---" , __HETAO_VERSION , __DATE__ , __TIME__ );
		SetHttpCloseExec( g_file_fd );
		
		/* 初始化服务器环境 */
		nret = InitEnvirment( p_env , p_conf ) ;
		free( p_conf );
		if( nret )
		{
			if( getenv( HETAO_LISTEN_SOCKFDS ) == NULL )
				printf( "InitEnvirment failed[%d]\n" , nret );
			return -nret;
		}
		
		return -BindDaemonServer( & MonitorProcess , p_env );
		/*
		return -MonitorProcess( p_env );
		*/
	}
	else
	{
		usage();
		exit(9);
	}
}

