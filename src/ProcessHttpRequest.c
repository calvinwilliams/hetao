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
		ErrorLog( __FILE__ , __LINE__ , "deflateInit2 failed , errno[%d]" , errno );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	in_len = html_content_len ;
	out_len = deflateBound( &stream , in_len ) ;
	out_base = (Bytef *)malloc( out_len+1 ) ;
	if( out_base == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
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
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
		free( out_base );
		deflateEnd( &stream );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	(*html_gzip_content) = (char*)out_base ;
	(*html_gzip_content_len) = stream.total_out ;
	
	return 0;
}

int ProcessHttpRequest( struct HetaoServer *p_server , struct HttpSession *p_http_session , char *pathname , char *filename , int filename_len )
{
	struct MimeType		*p_mimetype = NULL ;
	
	char			pathfilename[ 1024 + 1 ] ;
	int			pathfilename_len ;
	
	struct HtmlCacheSession	htmlcache_session ;
	struct HtmlCacheSession	*p_htmlcache_session = NULL , *p_bak = NULL ;
	
	struct HttpBuffer	*b = NULL ;
	char			*token_base = NULL ;
	char			*p_compress_algorithm = NULL ;
	int			compress_algorithm_len ;
	
	int			nret = 0 ;
	
	/* 查询流类型 */
	p_mimetype = QueryMimeTypeHashNode( p_server , p_http_session->http_uri.ext_filename_base , p_http_session->http_uri.ext_filename_len ) ;
	if( p_mimetype == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "QueryMimeTypeHashNode[%.*s] failed[%d]" , p_http_session->http_uri.ext_filename_len , p_http_session->http_uri.ext_filename_base , nret );
		return HTTP_FORBIDDEN;
	}
	
	/* 组装URL */
	memset( pathfilename , 0x00 , sizeof(pathfilename) );
	snprintf( pathfilename , sizeof(pathfilename)-1 , "%s%.*s" , pathname , filename_len , filename );
	if( strstr( pathfilename , ".." ) )
	{
		WarnLog( __FILE__ , __LINE__ , "URI[%s%.*s] include \"..\"" , pathname , filename_len , filename );
		return HTTP_BAD_REQUEST;
	}
	pathfilename_len = strlen(pathfilename) ;
	
	/* 查询网页缓存 */
	p_htmlcache_session = QueryHtmlCachePathfilenameTreeNode( p_server , pathfilename ) ;
	p_bak = p_htmlcache_session ;
	if( p_htmlcache_session == NULL )
	{
		/* 缓存里没有 */
		FILE		*fp = NULL ;
		
		/* 打开文件，或目录 */
		fp = fopen( pathfilename , "r" ) ;
		if( fp == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "fopen[%s] failed , errno[%d]" , pathfilename , errno );
			return HTTP_NOT_FOUND;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "fopen[%s] ok" , pathfilename );
		}
		
		p_htmlcache_session = & htmlcache_session ;
		memset( p_htmlcache_session , 0x00 , sizeof(struct HtmlCacheSession) );
				
		p_htmlcache_session->type = DATASESSION_TYPE_HTMLCACHE ;
		
		p_htmlcache_session->pathfilename = strdup( pathfilename ) ;
		if( p_htmlcache_session->pathfilename == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "strdup failed , errno[%d]" , errno );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		p_htmlcache_session->pathfilename_len = pathfilename_len ;
		
		/* 判断是目录还是文件 */
		fstat( fileno(fp) , & (p_htmlcache_session->st) );
		if( ! STAT_DIRECTORY(p_htmlcache_session->st) )
		{
			/* 是文件，读取整个文件 */
			p_htmlcache_session->html_content_len = (int)(p_htmlcache_session->st.st_size) ;
			p_htmlcache_session->html_content = (char*)malloc( p_htmlcache_session->html_content_len+1 ) ;
			if( p_htmlcache_session->html_content == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
				free( p_htmlcache_session->pathfilename );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			
			nret = fread( p_htmlcache_session->html_content , p_htmlcache_session->html_content_len , 1 , fp ) ;
			if( nret != 1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "fread failed , errno[%d]" , errno );
				free( p_htmlcache_session->pathfilename );
				free( p_htmlcache_session->html_content );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "fread[%s] ok , [%d]bytes" , p_htmlcache_session->pathfilename , p_htmlcache_session->html_content_len );
			}
		}
		
		/* 关闭文件，或目录 */
		fclose( fp );
		DebugLog( __FILE__ , __LINE__ , "fclose[%s] ok" , p_htmlcache_session->pathfilename );
	}
	else
	{
		/* 击中缓存 */
		DebugLog( __FILE__ , __LINE__ , "Html[%s] cache hited , [%d]bytes" , pathfilename , p_htmlcache_session->html_content_len );
	}
	
	if( STAT_DIRECTORY(p_htmlcache_session->st) )
	{
		/* 如果是目录，尝试所有索引文件 */
		char	index_copy[ sizeof(p_server->p_config->server.index) ] ;
		int	index_filename_len = 0 ;
		char	*index_filename = NULL ;
		
		if( p_server->p_config->server.index[0] == '\0' )
			return HTTP_NOT_FOUND;
		
		strcpy( index_copy , p_server->p_config->server.index );
		index_filename = strtok( index_copy , "," ) ;
		while( index_filename )
		{
			index_filename_len = strlen(index_filename) ;
			nret = ProcessHttpRequest( p_server , p_http_session , pathfilename , index_filename , index_filename_len ) ;
			if( nret == HTTP_OK )
				break;
			
			index_filename = strtok( NULL , "," ) ;
		}
		if( index_filename == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "wwwroot[%s] dirname[%.*s] index[%s] failed , errno[%d]" , p_server->p_config->server.wwwroot , index_filename_len , index_filename , p_server->p_config->server.index , errno );
			return HTTP_NOT_FOUND;
		}
	}
	else
	{
		/* 解析浏览器可以接受的压缩算法 */
		b = GetHttpResponseBuffer(p_http_session->http) ;
		token_base = QueryHttpHeaderPtr( p_http_session->http , HTTP_HEADER_ACCEPTENCODING , NULL ) ;
		while( token_base && p_server->p_config->http_options.compress_on )
		{
			token_base = TokenHttpHeaderValue( token_base , & p_compress_algorithm , & compress_algorithm_len ) ;
			if( p_compress_algorithm )
			{
				if( compress_algorithm_len == 4 && STRNICMP( p_compress_algorithm , == , "gzip" , compress_algorithm_len ) )
				{
					if( p_htmlcache_session->html_gzip_content == NULL )
					{
						/* 按gzip压缩 */
						nret = CompressData( p_htmlcache_session->html_content , p_htmlcache_session->html_content_len , HTTP_COMPRESSALGORITHM_GZIP , &(p_htmlcache_session->html_gzip_content) , &(p_htmlcache_session->html_gzip_content_len) ) ;
						if( nret )
						{
							ErrorLog( __FILE__ , __LINE__ , "CompressBuffer HTTP_COMPRESSALGORITHM_GZIP failed , errno[%d]" , errno );
							return nret;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "CompressBuffer HTTP_COMPRESSALGORITHM_GZIP ok , [%d]bytes" , p_htmlcache_session->html_gzip_content_len );
						}
					}
					else
					{
						DebugLog( __FILE__ , __LINE__ , "Gzip[%s] cache hited , [%d]bytes" , pathfilename , p_htmlcache_session->html_gzip_content_len );
					}
					
					/* 组织HTTP响应 */
					nret = StrcatfHttpBuffer( b ,	"Server: hetao/%s\r\n"
									"Content-Type: %s\r\n"
									"Content-Encoding: gzip\r\n"
									"Content-Length: %d\r\n"
									"%s"
									"\r\n"
									, __HETAO_VERSION
									, p_mimetype->mime
									, p_htmlcache_session->html_gzip_content_len
									, CheckHttpKeepAlive(p_http_session->http)?"Connection: Keep-Alive\r\n":"" ) ;
					if( nret )
					{
						ErrorLog( __FILE__ , __LINE__ , "StrcatfHttpBuffer failed , errno[%d]" , errno );
						return HTTP_INTERNAL_SERVER_ERROR;
					}
					
					if( GetHttpHeader_METHOD(p_http_session->http) == HTTP_METHOD_GET_N )
					{
						nret = MemcatHttpBuffer( b , p_htmlcache_session->html_gzip_content , p_htmlcache_session->html_gzip_content_len ) ;
						if( nret )
						{
							ErrorLog( __FILE__ , __LINE__ , "MemcatHttpBuffer failed , errno[%d]" , errno );
							return HTTP_INTERNAL_SERVER_ERROR;
						}
					}
					
					break;
				}
				else if( compress_algorithm_len == 7 && STRNICMP( p_compress_algorithm , == , "deflate" , compress_algorithm_len ) )
				{
					if( p_htmlcache_session->html_deflate_content == NULL )
					{
						/* 按deflate压缩 */
						nret = CompressData( p_htmlcache_session->html_content , p_htmlcache_session->html_content_len , HTTP_COMPRESSALGORITHM_DEFLATE , &(p_htmlcache_session->html_deflate_content) , &(p_htmlcache_session->html_deflate_content_len) ) ;
						if( nret )
						{
							ErrorLog( __FILE__ , __LINE__ , "CompressBuffer HTTP_COMPRESSALGORITHM_DEFLATE failed , errno[%d]" , errno );
							return nret;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "CompressBuffer HTTP_COMPRESSALGORITHM_DEFLATE ok , [%d]bytes" , p_htmlcache_session->html_deflate_content_len );
						}
					}
					else
					{
						DebugLog( __FILE__ , __LINE__ , "Deflate[%s] cache hited , [%d]bytes" , pathfilename , p_htmlcache_session->html_deflate_content_len );
					}
					
					/* 组织HTTP响应 */
					nret = StrcatfHttpBuffer( b ,	"Server: hetao/%s\r\n"
									"Content-Type: %s\r\n"
									"Content-Encoding: deflate\r\n"
									"Content-Length: %d\r\n"
									"%s"
									"\r\n"
									, __HETAO_VERSION
									, p_mimetype->mime
									, p_htmlcache_session->html_deflate_content_len
									, CheckHttpKeepAlive(p_http_session->http)?"Connection: Keep-Alive\r\n":"" ) ;
					if( nret )
					{
						ErrorLog( __FILE__ , __LINE__ , "StrcatfHttpBuffer failed , errno[%d]" , errno );
						return HTTP_INTERNAL_SERVER_ERROR;
					}
					
					if( GetHttpHeader_METHOD(p_http_session->http) == HTTP_METHOD_GET_N )
					{
						nret = MemcatHttpBuffer( b , p_htmlcache_session->html_deflate_content , p_htmlcache_session->html_deflate_content_len ) ;
						if( nret )
						{
							ErrorLog( __FILE__ , __LINE__ , "MemcatHttpBuffer failed , errno[%d]" , errno );
							return HTTP_INTERNAL_SERVER_ERROR;
						}
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
							, p_mimetype->mime
							, p_htmlcache_session->html_content_len
							, CheckHttpKeepAlive(p_http_session->http)?"Connection: Keep-Alive\r\n":"" ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "StrcatfHttpBuffer failed , errno[%d]" , errno );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			
			if( GetHttpHeader_METHOD(p_http_session->http) == HTTP_METHOD_GET_N )
			{
				nret = MemcatHttpBuffer( b , p_htmlcache_session->html_content , p_htmlcache_session->html_content_len ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "MemcatHttpBuffer failed , errno[%d]" , errno );
					return HTTP_INTERNAL_SERVER_ERROR;
				}
			}
		}
	}
	
	if( p_bak == NULL )
	{
		/* 缓存文件内容，并注册文件变动通知 */
		p_htmlcache_session = (struct HtmlCacheSession *)malloc( sizeof(struct HtmlCacheSession) ) ;
		if( p_htmlcache_session == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
			return -1;
		}
		memcpy( p_htmlcache_session , & htmlcache_session , sizeof(struct HtmlCacheSession) );
		
		p_htmlcache_session->wd = inotify_add_watch( p_server->htmlcache_inotify_fd , p_htmlcache_session->pathfilename , IN_MODIFY | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF ) ; 
		if( p_htmlcache_session->wd == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "inotify_add_watch[%s] failed , errno[%d]" , p_htmlcache_session->pathfilename , errno );
			FreeHtmlCacheSession( p_htmlcache_session );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "inotify_add_watch[%s] ok , wd[%d]" , p_htmlcache_session->pathfilename , p_htmlcache_session->wd );
		}
		
		nret = AddHtmlCacheWdTreeNode( p_server , p_htmlcache_session ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "AddHtmlCacheWdTreeNode failed , errno[%d]" , errno );
			inotify_rm_watch( p_server->htmlcache_inotify_fd , p_htmlcache_session->wd );
			FreeHtmlCacheSession( p_htmlcache_session );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		
		nret = AddHtmlCachePathfilenameTreeNode( p_server , p_htmlcache_session ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "AddHtmlCachePathfilenameTreeNode[%.*s] failed , errno[%d]" , p_htmlcache_session->pathfilename_len , p_htmlcache_session->pathfilename , errno );
			RemoveHtmlCacheWdTreeNode( p_server , p_htmlcache_session );
			inotify_rm_watch( p_server->htmlcache_inotify_fd , p_htmlcache_session->wd );
			FreeHtmlCacheSession( p_htmlcache_session );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "AddHtmlCachePathfilenameTreeNode[%.*s] ok" , p_htmlcache_session->pathfilename_len , p_htmlcache_session->pathfilename );
		}
		
		list_add( & (p_htmlcache_session->list) , & (p_server->htmlcache_session_list.list) );
		p_server->htmlcache_session_count++;
	}
	
	return HTTP_OK;
}

