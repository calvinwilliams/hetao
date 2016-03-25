/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

struct HetaoServer	*g_p_server = NULL ;

char	__HETAO_VERSION_0_1_0[] = "0.1.0" ;
char	*__HETAO_VERSION = __HETAO_VERSION_0_1_0 ;

static void usage()
{
	printf( "hetao v%s build %s %s\n" , __HETAO_VERSION , __DATE__ , __TIME__ );
	printf( "USAGE : hetao hetao.conf\n" );
	return;
}

int main( int argc , char *argv[] )
{
	struct HetaoServer	server ;
	struct HetaoServer	*p_server = NULL ;
	
	int			nret = 0 ;
	
	if( argc == 1 + 1 )
	{
		/* 重置所有HTTP状态码、描述为缺省 */
		ResetAllHttpStatus();
		
		p_server = & server ;
		g_p_server = p_server ;
		memset( p_server , 0x00 , sizeof(struct HetaoServer) );
		p_server->argv = argv ;
		
		/* 设置缺省主日志 */
		SetLogFile( "%s/log/error.log" , getenv("HOME") );
		SetLogLevel( LOGLEVEL_ERROR );
		SETPID
		SETTID
		UPDATEDATETIMECACHEFIRST
		
		/* 装载配置 */
		strncpy( p_server->config_pathfilename , argv[1] , sizeof(p_server->config_pathfilename)-1 );
		p_server->p_config = (hetao_conf *)malloc( sizeof(hetao_conf) ) ;
		if( p_server->p_config == NULL )
		{
			if( getenv( HETAO_LISTEN_SOCKFDS ) == NULL )
				printf( "alloc failed[%d] , errno[%d]\n" , nret , errno );
			return 1;
		}
		memset( p_server->p_config , 0x00 , sizeof(hetao_conf) );
		nret = LoadConfig( p_server->config_pathfilename , p_server ) ;
		if( nret )
		{
			if( getenv( HETAO_LISTEN_SOCKFDS ) == NULL )
				printf( "Load config failed[%d]\n" , nret );
			return -nret;
		}
		
		/* 重新设置主日志 */
		CloseLogFile();
		
		SetLogFile( p_server->p_config->error_log );
		SetLogLevel( p_server->log_level );
		SETPID
		SETTID
		UPDATEDATETIMECACHEFIRST
		InfoLog( __FILE__ , __LINE__ , "--- hetao v%s build %s %s ---" , __HETAO_VERSION , __DATE__ , __TIME__ );
		SetHttpCloseExec( g_file_fd );
		
		/* 初始化服务器环境 */
		nret = InitServerEnvirment( p_server ) ;
		if( nret )
		{
			if( getenv( HETAO_LISTEN_SOCKFDS ) == NULL )
				printf( "Init envirment failed[%d]\n" , nret );
			return -nret;
		}
		
		return -BindDaemonServer( & MonitorProcess , p_server );
		/*
		return -MonitorProcess( p_server );
		*/
	}
	else
	{
		usage();
		exit(9);
	}
}

