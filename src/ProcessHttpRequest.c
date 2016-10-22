/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

/* 压缩HTTP响应 */
static int CompressData( char *html_content , int html_content_len , int compress_algorithm , char **html_gzip_content , int *html_gzip_content_len )
{
	uLong		in_len ;
	uLong		out_len ;
	Bytef		*out_base = NULL ;
	z_stream	stream ;
	
	int		nret = 0 ;
	
	stream.zalloc = NULL ;
	stream.zfree = NULL ;
	stream.opaque = NULL ;
	nret = deflateInit2( &stream , Z_BEST_COMPRESSION , Z_DEFLATED , compress_algorithm , MAX_MEM_LEVEL , Z_DEFAULT_STRATEGY ) ;
	if( nret != Z_OK )
	{
		ErrorLog( __FILE__ , __LINE__ , "deflateInit2 failed , errno[%d]" , ERRNO );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	in_len = html_content_len ;
	out_len = deflateBound( &stream , in_len ) ;
	out_base = (Bytef *)malloc( out_len+1 ) ;
	if( out_base == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		deflateEnd( &stream );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	stream.next_in = (Bytef*)html_content ;
	stream.avail_in = in_len ;
	stream.next_out = out_base ;
	stream.avail_out = out_len ;
	nret = deflate( &stream , Z_FINISH ) ;
	if( nret != Z_OK && nret != Z_STREAM_END )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		free( out_base );
		deflateEnd( &stream );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	(*html_gzip_content) = (char*)out_base ;
	(*html_gzip_content_len) = stream.total_out ;
	
	return 0;
}

int ProcessHttpRequest( struct HetaoEnv *p_env , struct HttpSession *p_http_session , char *pathname , char *filename , int filename_len )
{
#if ( defined __linux ) || ( defined __unix )
	struct epoll_event	event ;
#elif ( defined _WIN32 )
#endif
	
	struct MimeType		*p_mime_type = NULL ;
	
	char			pathfilename[ 1024 + 1 ] ;
	int			pathfilename_len ;
	
	struct HtmlCacheSession	htmlcache_session ;
	struct HtmlCacheSession	*p_htmlcache_session = NULL , *p_bak = NULL ;
	
	struct HttpBuffer	*b = NULL ;
	char			*token_base = NULL ;
	char			*p_compress_algorithm = NULL ;
	int			compress_algorithm_len ;
	
	int			nret = 0 ;
	
	/* 分解URI */
	memset( & (p_http_session->http_uri) , 0x00 , sizeof(struct HttpUri) );
	nret = SplitHttpUri( pathname , filename , filename_len , & (p_http_session->http_uri) ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "SplitHttpUri[%s][%.*s] failed[%d] , errno[%d]" , pathname , filename_len , filename , nret , ERRNO );
		return HTTP_NOT_FOUND;
	}
	
	/* 如果文件类型为转发后端服务器 */
	if( p_http_session->p_virtual_host->forward_rule[0]
		&& p_http_session->http_uri.ext_filename_len == p_http_session->p_virtual_host->forward_type_len
		&& MEMCMP( p_http_session->http_uri.ext_filename_base , == , p_http_session->p_virtual_host->forward_type , p_http_session->http_uri.ext_filename_len ) )
	{
		while(1)
		{
			/* 选择转发服务端 */
			nret = SelectForwardAddress( p_env , p_http_session ) ;
			if( nret == HTTP_OK )
			{
				/* 连接转发服务端 */
				nret = ConnectForwardServer( p_env , p_http_session ) ;
				if( nret == HTTP_OK )
				{
#if ( defined __linux ) || ( defined __unix )
					/* 暂禁原连接事件 */
					memset( & event , 0x00 , sizeof(struct epoll_event) );
					event.events = EPOLLRDHUP | EPOLLERR ;
					event.data.ptr = p_http_session ;
					nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
					if( nret == -1 )
					{
						ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
						return -1;
					}
#endif
					
					return 0;
				}
				else
				{
					ErrorLog( __FILE__ , __LINE__ , "ConnectForwardServer failed[%d] , errno[%d]" , nret , ERRNO );
					return HTTP_SERVICE_UNAVAILABLE;
				}
			}
			else
			{
				ErrorLog( __FILE__ , __LINE__ , "SelectForwardAddress failed[%d] , errno[%d]" , nret , ERRNO );
				return HTTP_SERVICE_UNAVAILABLE;
			}
		}
	}
	
	/* 组装URL */
	memset( pathfilename , 0x00 , sizeof(pathfilename) );
	SNPRINTF( pathfilename , sizeof(pathfilename)-1 , "%s%.*s" , pathname , filename_len , filename );
	if( strstr( pathfilename , ".." ) )
	{
		WarnLog( __FILE__ , __LINE__ , "URI[%s%.*s] include \"..\"" , pathname , filename_len , filename );
		return HTTP_BAD_REQUEST;
	}
	pathfilename_len = strlen(pathfilename) ;
	
	/* 查询网页缓存 */
	p_htmlcache_session = QueryHtmlCachePathfilenameTreeNode( p_env , pathfilename ) ;
	p_bak = p_htmlcache_session ;
	if( p_htmlcache_session == NULL )
	{
		p_htmlcache_session = & htmlcache_session ;
		memset( p_htmlcache_session , 0x00 , sizeof(struct HtmlCacheSession) );
				
		p_htmlcache_session->type = DATASESSION_TYPE_HTMLCACHE ;
		
		p_htmlcache_session->pathfilename = STRDUP( pathfilename ) ;
		if( p_htmlcache_session->pathfilename == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "strdup failed , errno[%d]" , ERRNO );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		p_htmlcache_session->pathfilename_len = pathfilename_len ;
		
		/* 判断是目录还是文件 */
		STAT( pathfilename , & (p_htmlcache_session->st) );
		if( ! STAT_DIRECTORY(p_htmlcache_session->st) )
		{
			/* 缓存里没有 */
			FILE		*fp = NULL ;
			
			/* 打开文件，或目录 */
			fp = fopen( pathfilename , "rb" ) ;
			if( fp == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "fopen[%s] failed , errno[%d]" , pathfilename , ERRNO );
				return HTTP_NOT_FOUND;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "fopen[%s] ok" , pathfilename );
			}
			
			/* 是文件，读取整个文件 */
			p_htmlcache_session->html_content_len = (int)(p_htmlcache_session->st.st_size) ;
			p_htmlcache_session->html_content = (char*)malloc( p_htmlcache_session->html_content_len+1 ) ;
			if( p_htmlcache_session->html_content == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
				free( p_htmlcache_session->pathfilename );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			
			nret = fread( p_htmlcache_session->html_content , p_htmlcache_session->html_content_len , 1 , fp ) ;
			if( nret != 1 && p_htmlcache_session->html_content_len > 0 )
			{
				ErrorLog( __FILE__ , __LINE__ , "fread failed , errno[%d]" , ERRNO );
				free( p_htmlcache_session->pathfilename );
				free( p_htmlcache_session->html_content );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "fread[%s] ok , [%d]bytes" , p_htmlcache_session->pathfilename , p_htmlcache_session->html_content_len );
			}
			
			/* 关闭文件，或目录 */
			fclose( fp );
			DebugLog( __FILE__ , __LINE__ , "fclose[%s] ok" , p_htmlcache_session->pathfilename );
		}
	}
	else
	{
		/* 击中缓存 */
		DebugLog( __FILE__ , __LINE__ , "html[%s] cache hited , [%d]bytes" , pathfilename , p_htmlcache_session->html_content_len );
	}
	
	if( STAT_DIRECTORY(p_htmlcache_session->st) )
	{
		/* 如果是目录，尝试所有索引文件 */
		char	index_copy[ sizeof( ((hetao_conf*)0)->listen[0].website[0].index ) ] ;
		int	index_filename_len = 0 ;
		char	*index_filename = NULL ;
		
		if( p_http_session->p_virtual_host->index[0] == '\0' )
			return HTTP_NOT_FOUND;
		
		strcpy( index_copy , p_http_session->p_virtual_host->index );
		index_filename = strtok( index_copy , "," ) ;
		while( index_filename )
		{
			index_filename_len = strlen(index_filename) ;
			nret = ProcessHttpRequest( p_env , p_http_session , pathname , index_filename , index_filename_len ) ;
			if( nret == HTTP_OK )
				return HTTP_OK;
			else if( nret == 0 )
				return 0;
			
			index_filename = strtok( NULL , "," ) ;
		}
		if( index_filename == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "wwwroot[%s] dirname[%.*s] index[%s] failed , errno[%d]" , p_http_session->p_virtual_host->wwwroot , index_filename_len , index_filename , p_http_session->p_virtual_host->index , ERRNO );
			return HTTP_NOT_FOUND;
		}
	}
	else
	{
		/* 先格式化响应头首行，用成功状态码 */
		nret = FormatHttpResponseStartLine( HTTP_OK , p_http_session->http , 0 , NULL ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , ERRNO );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		
		/* 查询流类型 */
		p_mime_type = QueryMimeTypeHashNode( p_env , p_http_session->http_uri.ext_filename_base , p_http_session->http_uri.ext_filename_len ) ;
		if( p_mime_type == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "QueryMimeTypeHashNode[%.*s] failed[%d]" , p_http_session->http_uri.ext_filename_len , p_http_session->http_uri.ext_filename_base , nret );
			return HTTP_FORBIDDEN;
		}
		
		/* 解析浏览器可以接受的压缩算法 */
		b = GetHttpResponseBuffer(p_http_session->http) ;
		token_base = QueryHttpHeaderPtr( p_http_session->http , HTTP_HEADER_ACCEPTENCODING , NULL ) ;
		while( token_base && p_env->http_options__compress_on )
		{
			token_base = TokenHttpHeaderValue( token_base , & p_compress_algorithm , & compress_algorithm_len ) ;
			if( p_compress_algorithm )
			{
				if( p_mime_type->compress_enable == 1 && compress_algorithm_len == 4 && STRNICMP( p_compress_algorithm , == , "gzip" , compress_algorithm_len ) )
				{
					if( p_htmlcache_session->html_gzip_content == NULL )
					{
						/* 按gzip压缩 */
						nret = CompressData( p_htmlcache_session->html_content , p_htmlcache_session->html_content_len , HTTP_COMPRESSALGORITHM_GZIP , &(p_htmlcache_session->html_gzip_content) , &(p_htmlcache_session->html_gzip_content_len) ) ;
						if( nret )
						{
							ErrorLog( __FILE__ , __LINE__ , "CompressBuffer HTTP_COMPRESSALGORITHM_GZIP failed , errno[%d]" , ERRNO );
							return nret;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "CompressBuffer HTTP_COMPRESSALGORITHM_GZIP ok , [%d]bytes" , p_htmlcache_session->html_gzip_content_len );
						}
					}
					else
					{
						DebugLog( __FILE__ , __LINE__ , "gzip[%s] cache hited , [%d]bytes" , pathfilename , p_htmlcache_session->html_gzip_content_len );
					}
					
					/* 组织HTTP响应 */
					nret = StrcatfHttpBuffer( b ,	"Server: hetao/%s\r\n"
									"Content-Type: %s\r\n"
									"Content-Encoding: gzip\r\n"
									"Content-Length: %d\r\n"
									"%s"
									"\r\n"
									, __HETAO_VERSION
									, p_mime_type->mime
									, p_htmlcache_session->html_gzip_content_len
									, CheckHttpKeepAlive(p_http_session->http)?"Connection: Keep-Alive\r\n":"" ) ;
					if( nret )
					{
						ErrorLog( __FILE__ , __LINE__ , "StrcatfHttpBuffer failed , errno[%d]" , ERRNO );
						return HTTP_INTERNAL_SERVER_ERROR;
					}
					
					if( GetHttpHeader_METHOD(p_http_session->http) == HTTP_METHOD_GET_N )
					{
						/*
						nret = MemcatHttpBuffer( b , p_htmlcache_session->html_gzip_content , p_htmlcache_session->html_gzip_content_len ) ;
						if( nret )
						{
							ErrorLog( __FILE__ , __LINE__ , "MemcatHttpBuffer failed , errno[%d]" , ERRNO );
							return HTTP_INTERNAL_SERVER_ERROR;
						}
						*/
						SetHttpBufferPtr( p_http_session->http_buf , p_htmlcache_session->html_gzip_content_len+1 , p_htmlcache_session->html_gzip_content );
						AppendHttpBuffer( p_http_session->http , p_http_session->http_buf );
					}
					
					break;
				}
				else if( p_mime_type->compress_enable == 1 && compress_algorithm_len == 7 && STRNICMP( p_compress_algorithm , == , "deflate" , compress_algorithm_len ) )
				{
					if( p_htmlcache_session->html_deflate_content == NULL )
					{
						/* 按deflate压缩 */
						nret = CompressData( p_htmlcache_session->html_content , p_htmlcache_session->html_content_len , HTTP_COMPRESSALGORITHM_DEFLATE , &(p_htmlcache_session->html_deflate_content) , &(p_htmlcache_session->html_deflate_content_len) ) ;
						if( nret )
						{
							ErrorLog( __FILE__ , __LINE__ , "CompressBuffer HTTP_COMPRESSALGORITHM_DEFLATE failed , errno[%d]" , ERRNO );
							return nret;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "CompressBuffer HTTP_COMPRESSALGORITHM_DEFLATE ok , [%d]bytes" , p_htmlcache_session->html_deflate_content_len );
						}
					}
					else
					{
						DebugLog( __FILE__ , __LINE__ , "deflate[%s] cache hited , [%d]bytes" , pathfilename , p_htmlcache_session->html_deflate_content_len );
					}
					
					/* 组织HTTP响应 */
					nret = StrcatfHttpBuffer( b ,	"Server: hetao/%s\r\n"
									"Content-Type: %s\r\n"
									"Content-Encoding: deflate\r\n"
									"Content-Length: %d\r\n"
									"%s"
									"\r\n"
									, __HETAO_VERSION
									, p_mime_type->mime
									, p_htmlcache_session->html_deflate_content_len
									, CheckHttpKeepAlive(p_http_session->http)?"Connection: Keep-Alive\r\n":"" ) ;
					if( nret )
					{
						ErrorLog( __FILE__ , __LINE__ , "StrcatfHttpBuffer failed , errno[%d]" , ERRNO );
						return HTTP_INTERNAL_SERVER_ERROR;
					}
					
					if( GetHttpHeader_METHOD(p_http_session->http) == HTTP_METHOD_GET_N )
					{
						/*
						nret = MemcatHttpBuffer( b , p_htmlcache_session->html_deflate_content , p_htmlcache_session->html_deflate_content_len ) ;
						if( nret )
						{
							ErrorLog( __FILE__ , __LINE__ , "MemcatHttpBuffer failed , errno[%d]" , ERRNO );
							return HTTP_INTERNAL_SERVER_ERROR;
						}
						*/
						SetHttpBufferPtr( p_http_session->http_buf , p_htmlcache_session->html_deflate_content_len+1 , p_htmlcache_session->html_deflate_content );
						AppendHttpBuffer( p_http_session->http , p_http_session->http_buf );
					}
					
					break;
				}
			}
		}
		if( token_base == NULL )
		{
			/* 组织HTTP响应。未压缩 */
			nret = StrcatfHttpBuffer( b ,	"Server: hetao/%s\r\n"
							"Content-Type: %s\r\n"
							"Content-Length: %d\r\n"
							"%s"
							"\r\n"
							, __HETAO_VERSION
							, p_mime_type->mime
							, p_htmlcache_session->html_content_len
							, CheckHttpKeepAlive(p_http_session->http)?"Connection: Keep-Alive\r\n":"" ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "StrcatfHttpBuffer failed , errno[%d]" , ERRNO );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			
			if( GetHttpHeader_METHOD(p_http_session->http) == HTTP_METHOD_GET_N )
			{
				/*
				nret = MemcatHttpBuffer( b , p_htmlcache_session->html_content , p_htmlcache_session->html_content_len ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "MemcatHttpBuffer failed , errno[%d]" , ERRNO );
					return HTTP_INTERNAL_SERVER_ERROR;
				}
				*/
				SetHttpBufferPtr( p_http_session->http_buf , p_htmlcache_session->html_content_len+1 , p_htmlcache_session->html_content );
				AppendHttpBuffer( p_http_session->http , p_http_session->http_buf );
			}
		}
	}
	
	if( p_bak == NULL )
	{
		if( htmlcache_session.st.st_size <= p_env->limits__max_file_cache )
		{
			/* 缓存文件内容，并注册文件变动通知 */
			p_htmlcache_session = (struct HtmlCacheSession *)malloc( sizeof(struct HtmlCacheSession) ) ;
			if( p_htmlcache_session == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
				return -1;
			}
			memcpy( p_htmlcache_session , & htmlcache_session , sizeof(struct HtmlCacheSession) );
			
#if ( defined __linux ) || ( defined __unix )
			p_htmlcache_session->wd = inotify_add_watch( p_env->htmlcache_inotify_fd , p_htmlcache_session->pathfilename , IN_MODIFY | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF ) ; 
			if( p_htmlcache_session->wd == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "inotify_add_watch[%s] failed , errno[%d]" , p_htmlcache_session->pathfilename , ERRNO );
				FreeHtmlCacheSession( p_htmlcache_session , 1 );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "inotify_add_watch[%s] ok , wd[%d]" , p_htmlcache_session->pathfilename , p_htmlcache_session->wd );
			}
#elif ( defined _WIN32 )
#endif
			
			nret = AddHtmlCacheWdTreeNode( p_env , p_htmlcache_session ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "AddHtmlCacheWdTreeNode failed , errno[%d]" , ERRNO );
#if ( defined __linux ) || ( defined __unix )
				inotify_rm_watch( p_env->htmlcache_inotify_fd , p_htmlcache_session->wd );
#elif ( defined _WIN32 )
#endif
				FreeHtmlCacheSession( p_htmlcache_session , 1 );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			
			nret = AddHtmlCachePathfilenameTreeNode( p_env , p_htmlcache_session ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "AddHtmlCachePathfilenameTreeNode[%.*s] failed , errno[%d]" , p_htmlcache_session->pathfilename_len , p_htmlcache_session->pathfilename , ERRNO );
				RemoveHtmlCacheWdTreeNode( p_env , p_htmlcache_session );
#if ( defined __linux ) || ( defined __unix )
				inotify_rm_watch( p_env->htmlcache_inotify_fd , p_htmlcache_session->wd );
#elif ( defined _WIN32 )
#endif
				FreeHtmlCacheSession( p_htmlcache_session , 1 );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "AddHtmlCachePathfilenameTreeNode[%.*s] ok" , p_htmlcache_session->pathfilename_len , p_htmlcache_session->pathfilename );
			}
			
			list_add( & (p_htmlcache_session->list) , & (p_env->htmlcache_session_list.list) );
			p_env->htmlcache_session_count++;
		}
		else
		{
			FreeHtmlCacheSession( & htmlcache_session , 0 );
			DebugLog( __FILE__ , __LINE__ , "FreeHtmlCacheSession" );
		}
	}
	
	return HTTP_OK;
}

