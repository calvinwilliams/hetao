/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

struct HetaoEnv	*g_p_env = NULL ;

char	__HETAO_VERSION_0_12_5[] = "0.12.5" ;
char	*__HETAO_VERSION = __HETAO_VERSION_0_12_5 ;

char *strndup(const char *s, size_t n);

void SetDefaultConfig( hetao_conf *p_conf )
{
	p_conf->worker_processes = 1 ;
	p_conf->cpu_affinity = 1 ;
	p_conf->accept_mutex = 0 ;
	strcpy( p_conf->error_log , "/var/hetao/log/error.log" );
	strcpy( p_conf->log_level , "ERROR" );
	strcpy( p_conf->user , "nobody" );
	
	p_conf->limits.max_http_session_count = 100000 ;
	p_conf->limits.max_file_cache = 1024000 ;
	p_conf->limits.max_connections_per_ip = -1 ;
	
	p_conf->tcp_options.nodelay = 1 ;
	p_conf->tcp_options.nolinger = -1 ;
	
	p_conf->http_options.compress_on = 1 ;
	p_conf->http_options.timeout = 30 ;
	p_conf->http_options.elapse = 60 ;
	p_conf->http_options.forward_disable = 60 ;
	
	return;
}
	
void AppendDefaultConfig( hetao_conf *p_conf )
{
	if( p_conf->mime_types._mime_type_count == 0 )
	{
		strcpy( p_conf->mime_types.mime_type[0].type , "html htm shtml" );
		strcpy( p_conf->mime_types.mime_type[0].mime , "text/html" );
		p_conf->mime_types.mime_type[0].compress_enable = 1 ;
		strcpy( p_conf->mime_types.mime_type[1].type , "css" );
		strcpy( p_conf->mime_types.mime_type[1].mime , "text/css" );
		p_conf->mime_types.mime_type[1].compress_enable = 1 ;
		strcpy( p_conf->mime_types.mime_type[2].type , "xml" );
		strcpy( p_conf->mime_types.mime_type[2].mime , "text/xml" );
		p_conf->mime_types.mime_type[2].compress_enable = 1 ;
		strcpy( p_conf->mime_types.mime_type[3].type , "txt" );
		strcpy( p_conf->mime_types.mime_type[3].mime , "text/plain" );
		p_conf->mime_types.mime_type[3].compress_enable = 1 ;
		strcpy( p_conf->mime_types.mime_type[4].type , "gif" );
		strcpy( p_conf->mime_types.mime_type[4].mime , "image/gif" );
		p_conf->mime_types.mime_type[4].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[5].type , "jpeg jpg" );
		strcpy( p_conf->mime_types.mime_type[5].mime , "image/jpeg" );
		p_conf->mime_types.mime_type[5].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[6].type , "png" );
		strcpy( p_conf->mime_types.mime_type[6].mime , "image/png" );
		p_conf->mime_types.mime_type[6].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[7].type , "tif tiff" );
		strcpy( p_conf->mime_types.mime_type[7].mime , "image/tiff" );
		p_conf->mime_types.mime_type[7].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[8].type , "ico" );
		strcpy( p_conf->mime_types.mime_type[8].mime , "image/x-ico" );
		p_conf->mime_types.mime_type[8].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[9].type , "jng" );
		strcpy( p_conf->mime_types.mime_type[9].mime , "image/x-jng" );
		p_conf->mime_types.mime_type[9].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[10].type , "bmp" );
		strcpy( p_conf->mime_types.mime_type[10].mime , "image/x-ms-bmp" );
		p_conf->mime_types.mime_type[10].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[11].type , "svg svgz" );
		strcpy( p_conf->mime_types.mime_type[11].mime , "image/svg+xml" );
		p_conf->mime_types.mime_type[11].compress_enable = 1 ;
		strcpy( p_conf->mime_types.mime_type[12].type , "jar war ear" );
		strcpy( p_conf->mime_types.mime_type[12].mime , "application/java-archive" );
		p_conf->mime_types.mime_type[12].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[13].type , "json" );
		strcpy( p_conf->mime_types.mime_type[13].mime , "application/json" );
		p_conf->mime_types.mime_type[13].compress_enable = 1 ;
		strcpy( p_conf->mime_types.mime_type[14].type , "doc" );
		strcpy( p_conf->mime_types.mime_type[14].mime , "application/msword" );
		p_conf->mime_types.mime_type[14].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[15].type , "pdf" );
		strcpy( p_conf->mime_types.mime_type[15].mime , "application/pdf" );
		p_conf->mime_types.mime_type[15].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[16].type , "rtf" );
		strcpy( p_conf->mime_types.mime_type[16].mime , "application/rtf" );
		p_conf->mime_types.mime_type[16].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[17].type , "xls" );
		strcpy( p_conf->mime_types.mime_type[17].mime , "application/vnd.ms-excel" );
		p_conf->mime_types.mime_type[17].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[18].type , "ppt" );
		strcpy( p_conf->mime_types.mime_type[18].mime , "application/vnd.ms-powerpoint" );
		p_conf->mime_types.mime_type[18].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[19].type , "7z" );
		strcpy( p_conf->mime_types.mime_type[19].mime , "application/x-7z-compressed" );
		p_conf->mime_types.mime_type[19].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[20].type , "rar" );
		strcpy( p_conf->mime_types.mime_type[20].mime , "application/x-rar-compressed" );
		p_conf->mime_types.mime_type[20].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[21].type , "swf" );
		strcpy( p_conf->mime_types.mime_type[21].mime , "application/x-shockwave-flas" );
		p_conf->mime_types.mime_type[21].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[22].type , "xhtml" );
		strcpy( p_conf->mime_types.mime_type[22].mime , "application/xhtml+xml" );
		p_conf->mime_types.mime_type[22].compress_enable = 1 ;
		strcpy( p_conf->mime_types.mime_type[23].type , "bin exe dll iso img msi msp msm" );
		strcpy( p_conf->mime_types.mime_type[23].mime , "application/octet-stream" );
		p_conf->mime_types.mime_type[23].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[24].type , "zip" );
		strcpy( p_conf->mime_types.mime_type[24].mime , "application/zip" );
		p_conf->mime_types.mime_type[24].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[25].type , "docx" );
		strcpy( p_conf->mime_types.mime_type[25].mime , "application/vnd.openxmlformats-officedocument.wordprocessingml.document" );
		p_conf->mime_types.mime_type[25].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[26].type , "xlsx" );
		strcpy( p_conf->mime_types.mime_type[26].mime , "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" );
		p_conf->mime_types.mime_type[26].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[27].type , "pptx" );
		strcpy( p_conf->mime_types.mime_type[27].mime , "application/vnd.openxmlformats-officedocument.presentationml.presentation" );
		p_conf->mime_types.mime_type[27].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[28].type , "mid midi kar" );
		strcpy( p_conf->mime_types.mime_type[28].mime , "audio/midi" );
		p_conf->mime_types.mime_type[28].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[29].type , "mp3" );
		strcpy( p_conf->mime_types.mime_type[29].mime , "audio/mpeg" );
		p_conf->mime_types.mime_type[29].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[30].type , "ogg" );
		strcpy( p_conf->mime_types.mime_type[30].mime , "audio/ogg" );
		p_conf->mime_types.mime_type[30].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[31].type , "m4a" );
		strcpy( p_conf->mime_types.mime_type[31].mime , "audio/x-m4a" );
		p_conf->mime_types.mime_type[31].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[32].type , "ra" );
		strcpy( p_conf->mime_types.mime_type[32].mime , "audio/x-realaudio" );
		p_conf->mime_types.mime_type[32].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[33].type , "3gpp 3gp" );
		strcpy( p_conf->mime_types.mime_type[33].mime , "video/3gpp" );
		p_conf->mime_types.mime_type[33].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[34].type , "ts" );
		strcpy( p_conf->mime_types.mime_type[34].mime , "video/mp2t" );
		p_conf->mime_types.mime_type[34].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[35].type , "mp4" );
		strcpy( p_conf->mime_types.mime_type[35].mime , "video/mp4" );
		p_conf->mime_types.mime_type[35].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[36].type , "mpeg mpg" );
		strcpy( p_conf->mime_types.mime_type[36].mime , "video/mpeg" );
		p_conf->mime_types.mime_type[36].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[37].type , "mov" );
		strcpy( p_conf->mime_types.mime_type[37].mime , "video/quicktime" );
		p_conf->mime_types.mime_type[37].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[38].type , "webm" );
		strcpy( p_conf->mime_types.mime_type[38].mime , "video/webm" );
		p_conf->mime_types.mime_type[38].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[39].type , "flv" );
		strcpy( p_conf->mime_types.mime_type[39].mime , "video/x-flv" );
		p_conf->mime_types.mime_type[39].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[40].type , "m4v" );
		strcpy( p_conf->mime_types.mime_type[40].mime , "video/x-m4v" );
		p_conf->mime_types.mime_type[40].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[41].type , "mng" );
		strcpy( p_conf->mime_types.mime_type[41].mime , "video/x-mng" );
		p_conf->mime_types.mime_type[41].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[42].type , "asx asf" );
		strcpy( p_conf->mime_types.mime_type[42].mime , "video/x-ms-asf" );
		p_conf->mime_types.mime_type[42].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[43].type , "wmv" );
		strcpy( p_conf->mime_types.mime_type[43].mime , "video/x-ms-wmv" );
		p_conf->mime_types.mime_type[43].compress_enable = 0 ;
		strcpy( p_conf->mime_types.mime_type[44].type , "avi" );
		strcpy( p_conf->mime_types.mime_type[44].mime , "video/x-msvideo" );
		p_conf->mime_types.mime_type[44].compress_enable = 0 ;
		p_conf->mime_types._mime_type_count = 45 ;
	}
	
	return;
}

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
			ErrorLog( __FILE__ , __LINE__ , "strndup failed , errno[%d]" , ERRNO );
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
	
#if ( defined _WIN32 )
	for( p1 = buf ; (*p1) ; p1++ )
	{
		if( (*p1) == '\\' )
			(*p1) = '/' ;
	}
#endif
	
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
static char *StringExpandIncludeFile( char *config_pathfilename , char *buf , int *p_buf_size );

static char *StrdupEntireFile( char *pathfilename , int *p_file_size )
{
	struct stat	st ;
	char		*file_content = NULL ;
	int		file_size ;
	FILE		*fp = NULL ;
	
	char		*tmp = NULL ;
	
	int		nret = 0 ;
	
	nret = stat( pathfilename , & st ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "stat[%s] failed , errno[%d]" , pathfilename , ERRNO );
		return NULL;
	}
	file_size = (int)(st.st_size) ;
	
	file_content = (char*)malloc( file_size+1 ) ;
	if( file_content == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		return NULL;
	}
	memset( file_content , 0x00 , file_size+1 );
	
	fp = fopen( pathfilename , "rb" ) ;
	if( fp == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "fopen[%s] failed , errno[%d]" , pathfilename , ERRNO );
		return NULL;
	}
	
	nret = fread( file_content , file_size , 1 , fp ) ;
	if( nret != 1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "fread failed , errno[%d]" , ERRNO );
		return NULL;
	}
	
	fclose( fp );
	
	tmp = StringExpandIncludeFile( pathfilename , file_content , & file_size ) ;
	if( tmp == NULL )
	{
		free( file_content );
		return NULL;
	}
	file_content = tmp ;
	
	if( p_file_size )
		(*p_file_size) = file_size ;
	return file_content;
}

/* 把字符串中的!include文件展开 */
static char *StringExpandIncludeFile( char *config_pathfilename , char *buf , int *p_buf_size )
{
	char		include_config_pathfilename_base[ 256 + 1 ] ;
	char		include_config_pathfilename[ 256 + 1 ] ;
	char		*p = NULL ;
	
	char		*p1 = NULL ;
	char		*p2 = NULL ;
	char		*p3 = NULL ;
	char		*p4 = NULL ;
	int		offset ;
	int		len ;
	
	char		*p_include_file_content = NULL ;
	int		include_file_size ;
	
	char		*tmp = NULL ;
	
	memset( include_config_pathfilename_base , 0x00 , sizeof(include_config_pathfilename_base) );
	strcpy( include_config_pathfilename_base , config_pathfilename );
	
	p = strrchr( include_config_pathfilename_base , '/' ) ;
	if( p )
	{
		p[1] = '\0' ;
	}
	else
	{
		p = strrchr( include_config_pathfilename_base , '\\' ) ;
		if( p )
		{
			p[1] = '\0' ;
		}
		else
		{
			include_config_pathfilename_base[0] = '\0' ;
		}
	}
	
	while(1)
	{
		p1 = strstr( buf , "!include" ) ;
		if( p1 == NULL )
			break;
		p2 = p1 + strlen("!include") ;
		while( (*p2) )
		{
			if( (*p2) != ' ' )
				break;
			else
				p2++;
		}
		if( (*p2) == '\0' )
			return NULL;
		
		if( (*p2) == '"' )
		{
			p2++;
			p4 = strchr( p2 , '"' ) ;
			if( p4 == NULL )
				return NULL;
			p3 = p4 - 1 ;
			p4++;
		}
		else
		{
			p4 = strchr( p2 , '\n' ) ;
			if( p4 == NULL )
			{
				p4 = p2 + strlen(p2) ;
			}
			p3 = p4 - 1 ;
			while( p2 <= p3 )
			{
				if( ! strchr( " \r" , (*p3) ) )
					break;
				else
					p3--;
			}
			if( p2 > p3 )
				return NULL;
		}
		
		memset( include_config_pathfilename , 0x00 , sizeof(include_config_pathfilename) );
		strcpy( include_config_pathfilename , include_config_pathfilename_base );
		strncat( include_config_pathfilename , p2 , p3-p2+1 );
		p_include_file_content = StrdupEntireFile( include_config_pathfilename , & include_file_size ) ;
		if( p_include_file_content == NULL )
		{
			return NULL;
		}
		
		p = p_include_file_content ;
		while( p )
		{
			p = strpbrk( p , "\r\n" ) ;
			if( p )
			{
				(*p) = ' ' ;
				p++;
			}
		}
		
		offset = p1 - buf ;
		len = p4 - p1 ;
		
		tmp = (char*)realloc( buf , (*p_buf_size) + (include_file_size-len) + 1 ) ;
		if( tmp == NULL )
		{
			free( p_include_file_content );
			return NULL;
		}
		buf = tmp ;
		(*p_buf_size) += (include_file_size-len) ;
		
		/*
		aaa\n!include bbb\nccc
		     ^offset
		     |    len    |
		     p        p pp
		     1        2 34
		*/
		/*
		aaa\n!include "bbb"\nccc
		     ^offset
		     |    len    |
		     p         p p p
		     1         2 3 4
		*/
		memmove( buf+offset+len+(include_file_size-len) , buf+offset+len , strlen(buf+offset+len)+1 );
		memcpy( buf+offset , p_include_file_content , include_file_size );
		
		free( p_include_file_content );
	}
	
	return buf;
}

int LoadConfig( char *config_pathfilename , hetao_conf *p_config , struct HetaoEnv *p_env )
{
	char		*buf = NULL ;
	int		file_size ;
	
	int		nret = 0 ;
	
	/* 读取配置文件 */
	buf = StrdupEntireFile( config_pathfilename , & file_size ) ;
	if( buf == NULL )
		return -1;
	
	/* 解析配置文件 */
	nret = DSCDESERIALIZE_JSON_hetao_conf( NULL , buf , & file_size , p_config ) ;
	free( buf );
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "DSCDESERIALIZE_JSON_hetao_conf failed[%d][%d] , errno[%d]" , nret , DSCGetErrorLine_hetao_conf() , ERRNO );
		return -1;
	}
	
	return 0;
}
	
int ConvertConfig( hetao_conf *p_config , struct HetaoEnv *p_env )
{
	int		k , i ;
	
	int		nret = 0 ;
	
	/* 修正子进程数量 */
	if( p_config->worker_processes <= 0 )
	{
#if ( defined __linux ) || ( defined __unix )
		p_config->worker_processes = sysconf(_SC_NPROCESSORS_ONLN) ;
#elif ( defined _WIN32 )
		SYSTEM_INFO	systeminfo ;
		GetSystemInfo( & systeminfo );
		p_config->worker_processes = systeminfo.dwNumberOfProcessors ;
#endif
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
#if ( defined __linux ) || ( defined __unix )
		p_env->pwd = getpwnam( p_config->user ) ;
		if( p_env->pwd == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "user[%s] not found" , p_config->user );
			return nret;
		}
#elif ( defined _WIN32 )
#endif
	}
	
	/* 展开日志文件名中的环境变量 */
	if( p_config->_listen_count == 0 )
	{
		ErrorLog( __FILE__ , __LINE__ , "No listener" );
		return -1;
	}
	
	for( k = 0 ; k < p_config->_listen_count ; k++ )
	{
		if( p_config->listen[k]._website_count == 0 )
		{
			ErrorLog( __FILE__ , __LINE__ , "No website on listen[%d]" , p_config->listen[k].port );
			return -1;
		}
		
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
			
			nret = IsDirectory( p_config->listen[k].website[i].wwwroot ) ;
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
	strcpy( p_env->error_log , p_config->error_log );
	p_env->limits__max_http_session_count = p_config->limits.max_http_session_count ;
	p_env->limits__max_file_cache = p_config->limits.max_file_cache ;
	p_env->limits__max_connections_per_ip = p_config->limits.max_connections_per_ip ;
	p_env->tcp_options__nodelay = p_config->tcp_options.nodelay ;
	p_env->tcp_options__nolinger = p_config->tcp_options.nolinger ;
	p_env->http_options__timeout = p_config->http_options.timeout ;
	p_env->http_options__elapse = p_config->http_options.elapse ;
	p_env->http_options__compress_on = p_config->http_options.compress_on ;
	p_env->http_options__forward_disable = p_config->http_options.forward_disable ;
	
	return 0;
}

