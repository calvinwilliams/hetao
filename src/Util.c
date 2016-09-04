/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

/* 把当前进程转换为守护进程 */
int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv )
{
	int	pid;
	
	chdir( "/tmp" );
	
	pid = fork() ;
	switch( pid )
	{
		case -1:
			return -1;
		case 0:
			break;
		default		:
			exit( 0 );	
			break;
	}

	setsid() ;

	pid = fork() ;
	switch( pid )
	{
		case -1:
			return -2;
		case 0:
			break ;
		default:
			exit( 0 );
			break;
	}
	
	setuid( getpid() ) ;
	
	umask( 0 ) ;
	
	return ServerMain( pv );
}

/* 检查目录存在 */
int AccessDirectoryExist( char *pathdirname )
{
	struct stat	st ;
	int		nret = 0 ;
	
	nret = stat( pathdirname , & st ) ;
	if( nret == -1 )
		return -1;
	
	if( S_ISDIR(st.st_mode) )
		return 1;
	else
		return 0;
}

/* 检查文件存在 */
int AccessFileExist( char *pathfilename )
{
	int		nret = 0 ;
	
	nret = _ACCESS( pathfilename , _ACCESS_MODE ) ;
	if( nret == -1 )
		return -1;
	
	return 1;
}

/* 当前进程绑定CPU */
int BindCpuAffinity( int processor_no )
{
	cpu_set_t	cpu_mask ;
	
	int		nret = 0 ;
	
	CPU_ZERO( & cpu_mask );
	CPU_SET( processor_no , & cpu_mask );
	nret = sched_setaffinity( 0 , sizeof(cpu_mask) , & cpu_mask ) ;
	return nret;
}

/* 哈希函数 */
unsigned long CalcHash( char *str , int len )
{
	char		*p = str ;
	char		*p_end = str + len ;
	unsigned long	ul = 0 ;
	
	for( ; p < p_end ; p++ )
	{
		ul  = (*p) + (ul<<6)+ (ul>>16) - ul ;
	}
	
	return ul;
}

