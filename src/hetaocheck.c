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
	struct HetaoEnv		*p_env = NULL ;
	hetao_conf		*p_conf = NULL ;
	
	int			nret = 0 ;
	
	UMASK(0);
	
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
			free( p_env );
			return 1;
		}
		memset( p_conf , 0x00 , sizeof(hetao_conf) );
		
		/* 设置缺省主日志 */
		SetLogFile( "#" );
		SetLogLevel( LOGLEVEL_ERROR );
		SETPID
		SETTID
		UPDATEDATETIMECACHEFIRST
		
		/* 设置缺省配置 */
		SetDefaultConfig( p_conf );
		
		/* 装载配置 */
		strncpy( p_env->config_pathfilename , argv[1] , sizeof(p_env->config_pathfilename)-1 );
		nret = LoadConfig( p_env->config_pathfilename , p_conf , p_env ) ;
		if( nret )
		{
			printf( "FAILED[%d]\n" , nret );
			free( p_conf );
			free( p_env );
			return 1;
		}
		
		/* 追加缺省配置 */
		AppendDefaultConfig( p_conf );
		
		/* 转换配置 */
		nret = ConvertConfig( p_conf , p_env ) ;
		if( nret )
		{
			printf( "FAILED[%d]\n" , nret );
			free( p_conf );
			free( p_env );
			return 1;
		}
		
		printf( "OK\n" );
		
		/* 释放环境 */
		free( p_conf );
		free( p_env );
		
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

