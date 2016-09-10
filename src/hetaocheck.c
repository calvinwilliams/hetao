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
	printf( "hetaocheck v%s build %s %s\n" , __HETAO_VERSION , __DATE__ , __TIME__ );
	printf( "USAGE : hetaocheck hetao.conf\n" );
	return;
}

int main( int argc , char *argv[] )
{
	struct HetaoEnv		server ;
	struct HetaoEnv		*p_env = NULL ;
	
	int			nret = 0 ;
	
	umask(0);
	
	if( argc == 1 + 1 )
	{
		p_env = & server ;
		g_p_env = p_env ;
		memset( p_env , 0x00 , sizeof(struct HetaoEnv) );
		
		/* 设置缺省主日志 */
		SetLogFile( "#" );
		SetLogLevel( LOGLEVEL_ERROR );
		SETPID
		SETTID
		UPDATEDATETIMECACHEFIRST
		
		/* 装载配置 */
		strncpy( p_env->config_pathfilename , argv[1] , sizeof(p_env->config_pathfilename)-1 );
		p_env->p_config = (hetao_conf *)malloc( sizeof(hetao_conf) ) ;
		if( p_env->p_config == NULL )
		{
			if( getenv( HETAO_LISTEN_SOCKFDS ) == NULL )
				printf( "alloc failed[%d] , errno[%d]\n" , nret , errno );
			return 1;
		}
		memset( p_env->p_config , 0x00 , sizeof(hetao_conf) );
		nret = LoadConfig( p_env->config_pathfilename , p_env ) ;
		if( nret )
		{
			printf( "FAILED[%d]\n" , nret );
		}
		else
		{
			printf( "OK\n" );
		}
		
		free( p_env->p_config );
		
		/* 关闭主日志 */
		CloseLogFile();
		
		return -nret;
	}
	else
	{
		usage();
		exit(9);
	}
}

