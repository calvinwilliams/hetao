/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

struct HetaoEnv	*g_p_env = NULL ;

char	__HETAO_VERSION_0_7_3[] = "0.7.3" ;
char	*__HETAO_VERSION = __HETAO_VERSION_0_7_3 ;

char *strndup(const char *s, size_t n);

/* 把字符串中的$...$用环境变量替换 */
static int StringExpandEnvval( char *buf , int buf_size )
{
	int		total_len ;
	char		*p1 = NULL ;
	char		*p2 = NULL ;
	char		*env_name = NULL ;
	char		*env_value = NULL ;
	int		env_value_len ;
	
	total_len = strlen( buf ) ;
	p1 = buf ;
	while(1)
	{
		p1 = strchr( p1 , '$' ) ;
		if( p1 == NULL )
			break;
		p2 = strchr( p1+1 , '$' ) ;
		if( p2 == NULL )
			break;
		
		env_name = strndup( p1+1 , p2-p1-1 ) ;
		if( env_name == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "strndup failed , errno[%d]" , errno );
			return -1;
		}
		
		env_value = getenv( env_name ) ;
		if( env_value == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "getenv[%s] failed , errno[%d]" , env_name );
			free( env_name );
			return -1;
		}
		
		/*
		p    p
		1    2
		$HOME$/log/access.log
		/home/calvin/log/access.log
		*/
		env_value_len = strlen( env_value ) ;
		if( total_len + ( env_value_len - (p2-p1+1) ) > buf_size-1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "buf[%s] replace overflow" , buf );
			free( env_name );
			return -1;
		}
		memmove( p1+env_value_len , p2+1 , strlen(p2+1)+1 );
		memcpy( p1 , env_value , env_value_len );
		
		free( env_name );
	}
	
	return 0;
}

/* 把字符串型的日志等级转换为整型 */
static int ConvertLogLevel_atoi( char *log_level_desc , int *p_log_level )
{
	if( strcmp( log_level_desc , "DEBUG" ) == 0 )
		(*p_log_level) = LOGLEVEL_DEBUG ;
	else if( strcmp( log_level_desc , "INFO" ) == 0 )
		(*p_log_level) = LOGLEVEL_INFO ;
	else if( strcmp( log_level_desc , "WARN" ) == 0 )
		(*p_log_level) = LOGLEVEL_WARN ;
	else if( strcmp( log_level_desc , "ERROR" ) == 0 )
		(*p_log_level) = LOGLEVEL_ERROR ;
	else if( strcmp( log_level_desc , "FATAL" ) == 0 )
		(*p_log_level) = LOGLEVEL_FATAL ;
	else
		return -11;
	
	return 0;
}

/* 装载整个文件 */
/* 内有申请内存，请注意后续释放 */
static char *StrdupEntireFile( char *pathfilename , int *p_file_size )
{
	struct stat	st ;
	char		*p_html_content = NULL ;
	FILE		*fp = NULL ;
	int		nret = 0 ;
	
	nret = stat( pathfilename , & st ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "stat[%s] failed , errno[%d]" , pathfilename , errno );
		return NULL;
	}
	
	p_html_content = (char*)malloc( st.st_size+1 ) ;
	if( p_html_content == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
		return NULL;
	}
	memset( p_html_content , 0x00 , st.st_size+1 );
	
	fp = fopen( pathfilename , "r" ) ;
	if( fp == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "fopen[%s] failed , errno[%d]" , pathfilename , errno );
		return NULL;
	}
	
	nret = fread( p_html_content , st.st_size , 1 , fp ) ;
	if( nret != 1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "fread failed , errno[%d]" , errno );
		return NULL;
	}
	
	fclose( fp );
	
	if( p_file_size )
		(*p_file_size) = (int)(st.st_size) ;
	return p_html_content;
}

int LoadConfig( char *config_pathfilename , hetao_conf *p_config , struct HetaoEnv *p_env )
{
	char		*buf = NULL ;
	int		file_size ;
	int		k , i ;
	
	int		nret = 0 ;
	
	/* 读取配置文件 */
	buf = StrdupEntireFile( config_pathfilename , & file_size ) ;
	if( buf == NULL )
		return -1;
	
	/* 预设缺省值 */
	p_config->worker_processes = sysconf(_SC_NPROCESSORS_ONLN) ;
	
	p_config->limits.max_http_session_count = MAX_HTTP_SESSION_COUNT_DEFAULT ;
	
	/* 解析配置文件 */
	nret = DSCDESERIALIZE_JSON_hetao_conf( NULL , buf , & file_size , p_config ) ;
	free( buf );
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "DSCDESERIALIZE_JSON_hetao_conf failed[%d][%d] , errno[%d]" , nret , DSCGetErrorLine_hetao_conf() , errno );
		return -1;
	}
	
	if( p_config->worker_processes <= 0 )
	{
		p_config->worker_processes = sysconf(_SC_NPROCESSORS_ONLN) ;
	}
	
	/* 展开主日志名中的环境变量 */
	nret = StringExpandEnvval( p_config->error_log , sizeof(p_config->error_log) ) ;
	if( nret )
		return nret;
	
	/* 转换主日志等级值 */
	nret = ConvertLogLevel_atoi( p_config->log_level , &(p_env->log_level) ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "log_level[%s] invalid" , p_config->log_level );
		return nret;
	}
	
	/* 检查用户名 */
	if( p_config->user[0] )
	{
		p_env->pwd = getpwnam( p_config->user ) ;
		if( p_env->pwd == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "user[%s] not found" , p_config->user );
			return nret;
		}
	}
	
	/* 展开日志文件名中的环境变量 */
	for( k = 0 ; k < p_config->_listen_count ; k++ )
	{
		/* 展开SSL证书文件名中的环境变量 */
		nret = StringExpandEnvval( p_config->listen[k].ssl.certificate_file , sizeof(p_config->listen[k].ssl.certificate_file) ) ;
		if( nret )
			return nret;
		
		if( p_config->listen[k].ssl.certificate_file[0] )
		{
			nret = AccessFileExist( p_config->listen[k].ssl.certificate_file ) ;
			if( nret != 1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "ssl.certificate_file[%s] not exist" , p_config->listen[k].ssl.certificate_file , nret );
				return -1;
			}
		}
		
		nret = StringExpandEnvval( p_config->listen[k].ssl.certificate_key_file , sizeof(p_config->listen[k].ssl.certificate_key_file) ) ;
		if( nret )
			return nret;
		
		if( p_config->listen[k].ssl.certificate_key_file[0] )
		{
			nret = AccessFileExist( p_config->listen[k].ssl.certificate_key_file ) ;
			if( nret != 1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "ssl.certificate_key_file[%s] not exist" , p_config->listen[k].ssl.certificate_key_file , nret );
				return -1;
			}
		}
		
		for( i = 0 ; i < p_config->listen[k]._website_count ; i++ )
		{
			nret = StringExpandEnvval( p_config->listen[k].website[i].wwwroot , sizeof(p_config->listen[k].website[i].wwwroot) ) ;
			if( nret )
				return nret;
			
			nret = AccessDirectoryExist( p_config->listen[k].website[i].wwwroot ) ;
			if( nret != 1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "wwwroot[%s] not exist" , p_config->listen[k].website[i].wwwroot , nret );
				return -1;
			}
			
			nret = StringExpandEnvval( p_config->listen[k].website[i].access_log , sizeof(p_config->listen[k].website[i].access_log) ) ;
			if( nret )
				return nret;
			
			if( p_config->listen[k].website[i].forward.forward_rule[0] )
			{
				nret = StringExpandEnvval( p_config->listen[k].website[i].forward.ssl.certificate_file , sizeof(p_config->listen[k].website[i].forward.ssl.certificate_file) ) ;
				if( nret )
					return nret;
				
				if( p_config->listen[k].website[i].forward.ssl.certificate_file[0] )
				{
					nret = AccessFileExist( p_config->listen[k].website[i].forward.ssl.certificate_file ) ;
					if( nret != 1 )
					{
						ErrorLog( __FILE__ , __LINE__ , "ssl.certificate_file[%s] not exist" , p_config->listen[k].website[i].forward.ssl.certificate_file , nret );
						return -1;
					}
				}
				
				if( STRCMP( p_config->listen[k].website[i].forward.forward_rule , != , FORWARD_RULE_ROUNDROBIN )
					&& STRCMP( p_config->listen[k].website[i].forward.forward_rule , != , FORWARD_RULE_LEASTCONNECTION )
				)
				{
					ErrorLog( __FILE__ , __LINE__ , "p_config->server.forward.forward_rule[%s] invalid" , p_config->listen[k].website[i].forward.forward_rule );
					return -1;
				}
			}
		}
	}
	
	/* 设置个性化页面信息 */
	if( STRCMP( p_config->error_pages.error_page_400 , != , "" ) )
	{
		char	*p_html_content = NULL ;
		
		nret = StringExpandEnvval( p_config->error_pages.error_page_400 , sizeof(p_config->error_pages.error_page_400) ) ;
		if( nret )
			return nret;
		
		p_html_content = StrdupEntireFile( p_config->error_pages.error_page_400 , NULL ) ;
		if( p_html_content == NULL )
			return -1;
		
		SetHttpStatus( HTTP_BAD_REQUEST , HTTP_BAD_REQUEST_S , p_html_content );
	}
	
	if( STRCMP( p_config->error_pages.error_page_401 , != , "" ) )
	{
		char	*p_html_content = NULL ;
		
		nret = StringExpandEnvval( p_config->error_pages.error_page_401 , sizeof(p_config->error_pages.error_page_401) ) ;
		if( nret )
			return nret;
		
		p_html_content = StrdupEntireFile( p_config->error_pages.error_page_401 , NULL ) ;
		if( p_html_content == NULL )
			return -1;
		
		SetHttpStatus( HTTP_UNAUTHORIZED , HTTP_UNAUTHORIZED_S , p_html_content );
	}
	
	if( STRCMP( p_config->error_pages.error_page_403 , != , "" ) )
	{
		char	*p_html_content = NULL ;
		
		nret = StringExpandEnvval( p_config->error_pages.error_page_403 , sizeof(p_config->error_pages.error_page_403) ) ;
		if( nret )
			return nret;
		
		p_html_content = StrdupEntireFile( p_config->error_pages.error_page_403 , NULL ) ;
		if( p_html_content == NULL )
			return -1;
		
		SetHttpStatus( HTTP_FORBIDDEN , HTTP_FORBIDDEN_S , p_html_content );
	}
	
	if( STRCMP( p_config->error_pages.error_page_404 , != , "" ) )
	{
		char	*p_html_content = NULL ;
		
		nret = StringExpandEnvval( p_config->error_pages.error_page_404 , sizeof(p_config->error_pages.error_page_404) ) ;
		if( nret )
			return nret;
		
		p_html_content = StrdupEntireFile( p_config->error_pages.error_page_404 , NULL ) ;
		if( p_html_content == NULL )
			return -1;
		
		SetHttpStatus( HTTP_NOT_FOUND , HTTP_NOT_FOUND_S , p_html_content );
	}
	
	if( STRCMP( p_config->error_pages.error_page_408 , != , "" ) )
	{
		char	*p_html_content = NULL ;
		
		nret = StringExpandEnvval( p_config->error_pages.error_page_408 , sizeof(p_config->error_pages.error_page_408) ) ;
		if( nret )
			return nret;
		
		p_html_content = StrdupEntireFile( p_config->error_pages.error_page_408 , NULL ) ;
		if( p_html_content == NULL )
			return -1;
		
		SetHttpStatus( HTTP_REQUEST_TIMEOUT , HTTP_REQUEST_TIMEOUT_S , p_html_content );
	}
	
	if( STRCMP( p_config->error_pages.error_page_500 , != , "" ) )
	{
		char	*p_html_content = NULL ;
		
		nret = StringExpandEnvval( p_config->error_pages.error_page_500 , sizeof(p_config->error_pages.error_page_500) ) ;
		if( nret )
			return nret;
		
		p_html_content = StrdupEntireFile( p_config->error_pages.error_page_500 , NULL ) ;
		if( p_html_content == NULL )
			return -1;
		
		SetHttpStatus( HTTP_INTERNAL_SERVER_ERROR , HTTP_INTERNAL_SERVER_ERROR_S , p_html_content );
	}
	
	if( STRCMP( p_config->error_pages.error_page_503 , != , "" ) )
	{
		char	*p_html_content = NULL ;
		
		nret = StringExpandEnvval( p_config->error_pages.error_page_503 , sizeof(p_config->error_pages.error_page_503) ) ;
		if( nret )
			return nret;
		
		p_html_content = StrdupEntireFile( p_config->error_pages.error_page_503 , NULL ) ;
		if( p_html_content == NULL )
			return -1;
		
		SetHttpStatus( HTTP_SERVICE_UNAVAILABLE , HTTP_SERVICE_UNAVAILABLE_S , p_html_content );
	}
	
	if( STRCMP( p_config->error_pages.error_page_505 , != , "" ) )
	{
		char	*p_html_content = NULL ;
		
		nret = StringExpandEnvval( p_config->error_pages.error_page_505 , sizeof(p_config->error_pages.error_page_505) ) ;
		if( nret )
			return nret;
		
		p_html_content = StrdupEntireFile( p_config->error_pages.error_page_505 , NULL ) ;
		if( p_html_content == NULL )
			return -1;
		
		SetHttpStatus( HTTP_VERSION_NOT_SUPPORTED , HTTP_VERSION_NOT_SUPPORTED_S , p_html_content );
	}
	
	/* 填充环境结构 */
	p_env->worker_processes = p_config->worker_processes ;
	p_env->cpu_affinity = p_config->cpu_affinity ;
	p_env->accept_mutex = p_config->accept_mutex ;
	p_env->limits__max_http_session_count = p_config->limits.max_http_session_count ;
	p_env->limits__max_file_cache = p_config->limits.max_file_cache ;
	p_env->tcp_options__nodelay = p_config->tcp_options.nodelay ;
	p_env->tcp_options__nolinger = p_config->tcp_options.nolinger ;
	p_env->http_options__timeout = p_config->http_options.timeout ;
	p_env->http_options__compress_on = p_config->http_options.compress_on ;
	p_env->http_options__forward_disable = p_config->http_options.forward_disable ;
	
	return 0;
}

