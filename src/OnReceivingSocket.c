/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int OnReceivingSocket( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
	struct HttpBuffer	*request_buf = NULL ;
	struct HttpBuffer	*response_buf = NULL ;
	struct HttpBuffer	*append_buf = NULL ;
	
	int			nret = 0 ;
	
#if ( defined __linux ) || ( defined __unix )
	struct epoll_event	event ;
	
	/* 收一把HTTP请求 */
	nret = ReceiveHttpRequestNonblock( p_http_session->netaddr.sock , p_http_session->ssl , p_http_session->http ) ;
	if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
	{
		/* 没收完整 */
		DebugLog( __FILE__ , __LINE__ , "ReceiveHttpRequestNonblock return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER" );
		
		UpdateHttpSessionTimeoutTreeNode( p_env , p_http_session , GETSECONDSTAMP + p_env->http_options__timeout );
	}
	else if( nret )
	{
		/* 接收报错了 */
		if( nret == FASTERHTTP_ERROR_TCP_CLOSE )
		{
			ErrorLog( __FILE__ , __LINE__ , "http socket closed detected" );
			return 1;
		}
		else if( nret == FASTERHTTP_INFO_TCP_CLOSE )
		{
			InfoLog( __FILE__ , __LINE__ , "http socket closed detected" );
			return 1;
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__ , "ReceiveHttpRequestNonblock failed[%d] , errno[%d]" , nret , ERRNO );
			return 1;
		}
	}
#elif ( defined _WIN32 )
	WSABUF			buf ;
	DWORD			dwFlags ;
	
	/* 解析一把HTTP请求 */
	nret = ParseHttpRequest( p_http_session->http ) ;
	if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
	{
		/* 没收完整 */
		DebugLog( __FILE__ , __LINE__ , "ParseHttpRequest return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER" );
		
		UpdateHttpSessionTimeoutTreeNode( p_env , p_http_session , (int)GETSECONDSTAMP + p_env->http_options__timeout );
		
		/* 继续投递接收事件 */
		if( p_http_session->ssl == NULL )
		{
			request_buf = GetHttpRequestBuffer( p_http_session->http );
			buf.buf = GetHttpBufferBase( request_buf , NULL ) + GetHttpBufferLength( request_buf ) ;
			buf.len = GetHttpBufferSize( request_buf ) - 1 - GetHttpBufferLength( request_buf ) ;
		}
		else
		{
			buf.buf = p_http_session->in_bio_buffer ;
			buf.len = sizeof(p_http_session->in_bio_buffer) - 1 ;
		}
		dwFlags = 0 ;
		nret = WSARecv( p_http_session->netaddr.sock , & buf , 1 , NULL , & dwFlags , & (p_http_session->overlapped) , NULL ) ;
		if( nret == SOCKET_ERROR )
		{
			if( WSAGetLastError() == ERROR_IO_PENDING )
			{
				DebugLog( __FILE__ , __LINE__ , "WSARecv io pending" );
			}
			else
			{
				ErrorLog( __FILE__ , __LINE__ , "WSARecv failed , errno[%d]" , ERRNO );
				return 1;
			}
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "WSARecv ok" );
		}
	}
	else if( nret )
	{
		/* 接收报错了 */
		if( nret == FASTERHTTP_ERROR_TCP_CLOSE )
		{
			ErrorLog( __FILE__ , __LINE__ , "http socket closed detected" );
			return 1;
		}
		else if( nret == FASTERHTTP_INFO_TCP_CLOSE )
		{
			InfoLog( __FILE__ , __LINE__ , "http socket closed detected" );
			return 1;
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__ , "ParseHttpRequest failed[%d] , errno[%d]" , nret , ERRNO );
			return 1;
		}
	}
#endif
	else
	{
		/* 收满一个HTTP请求 */
		char			*host = NULL ;
		int			host_len ;
		
		struct RewriteUri	*p_rewrite_uri = NULL ;
		struct RedirectDomain	*p_redirect_domain = NULL ;
		char			*p_uri = NULL ;
		char			uri[ 4096 + 1 ] ;
		int			uri_len ;
		
		DebugLog( __FILE__ , __LINE__ , "ReceiveHttpRequestNonblock done" );
		
		request_buf = GetHttpRequestBuffer(p_http_session->http) ;
		DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(request_buf,NULL) , GetHttpBufferLength(request_buf) , "HttpRequestBuffer [%d]bytes" , GetHttpBufferLength(request_buf) );
		response_buf = GetHttpResponseBuffer(p_http_session->http) ;
		
		/* 查询虚拟主机 */
		host = QueryHttpHeaderPtr( p_http_session->http , "Host" , & host_len ) ;
		if( host == NULL )
			host = "" , host_len = 0 ;
		p_http_session->p_virtual_host = QueryVirtualHostHashNode( p_http_session->p_listen_session , host , host_len ) ;
		if( p_http_session->p_virtual_host == NULL && p_http_session->p_listen_session->p_virtual_host_default )
		{
			p_http_session->p_virtual_host = p_http_session->p_listen_session->p_virtual_host_default ;
		}
		
		/* 处理HTTP请求 */
		if( p_http_session->p_virtual_host )
		{
			DebugLog( __FILE__ , __LINE__ , "QueryVirtualHostHashNode[%.*s] ok , wwwroot[%s]" , host_len , host , p_http_session->p_virtual_host->wwwroot );
			
			/* 重写地址处理 */
			if( p_env->new_uri_re == NULL )
			{
				p_uri = GetHttpHeaderPtr_URI(p_http_session->http,NULL) ;
				uri_len = GetHttpHeaderLen_URI(p_http_session->http) ;
			}
			else
			{
				p_uri = NULL ;
				list_for_each_entry( p_rewrite_uri , & (p_http_session->p_virtual_host->rewrite_uri_list.rewrite_uri_node) , struct RewriteUri , rewrite_uri_node )
				{
					strcpy( uri , p_rewrite_uri->new_uri );
					uri_len = p_rewrite_uri->new_uri_len ;
					
					nret = RegexReplaceString( p_rewrite_uri->pattern_re , GetHttpHeaderPtr_URI(p_http_session->http,NULL) , GetHttpHeaderLen_URI(p_http_session->http) , p_env->new_uri_re , uri , & uri_len , sizeof(uri) ) ;
					if( nret == 0 )
					{
						DebugLog( __FILE__ , __LINE__ , "RegexReplaceString[%.*s][%s][%s] ok[%.*s]" , GetHttpHeaderLen_URI(p_http_session->http) , GetHttpHeaderPtr_URI(p_http_session->http,NULL) , p_rewrite_uri->pattern , p_rewrite_uri->new_uri , uri_len , uri );
						p_uri = uri ;
						break;
					}
					else if( nret == -1 )
					{
						ErrorLog( __FILE__ , __LINE__ , "RegexReplaceString[%.*s][%s][%s] failed[%d] , errno[%d]" , GetHttpHeaderLen_URI(p_http_session->http) , GetHttpHeaderPtr_URI(p_http_session->http,NULL) , p_rewrite_uri->pattern , p_rewrite_uri->new_uri , nret , ERRNO );
						return HTTP_BAD_REQUEST;
					}
					else
					{
						DebugLog( __FILE__ , __LINE__ , "RegexReplaceString[%.*s][%s][%s] continue" , GetHttpHeaderLen_URI(p_http_session->http) , GetHttpHeaderPtr_URI(p_http_session->http,NULL) , p_rewrite_uri->pattern , p_rewrite_uri->new_uri );
					}
				}
				if( p_uri == NULL )
				{
					p_uri = GetHttpHeaderPtr_URI(p_http_session->http,NULL) ;
					uri_len = GetHttpHeaderLen_URI(p_http_session->http) ;
				}
			}
			
#if ( defined _WIN32 )
			if( p_uri[uri_len-1] == '/' || p_uri[uri_len-1] == '\\' )
				uri_len--;
#endif
			
			/* 重定向域名处理 */
			if( p_http_session->p_virtual_host->redirect_domain_count > 0 )
			{
				p_redirect_domain = QueryRedirectDomainHashNode( p_http_session->p_virtual_host , host , host_len ) ;
			}
			if( p_redirect_domain == NULL && p_http_session->p_virtual_host->p_redirect_domain_default )
				p_redirect_domain = p_http_session->p_virtual_host->p_redirect_domain_default ;
			if( p_redirect_domain )
			{
				nret = FormatHttpResponseStartLine( HTTP_MOVED_PERMANNETLY , p_http_session->http , 0 , "Location: %s" HTTP_RETURN_NEWLINE HTTP_RETURN_NEWLINE , p_redirect_domain->new_domain ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , ERRNO );
					return 1;
				}
				
				SetHttpKeepAlive( p_http_session->http , 0 );
			}
			else
			{
				/* 处理HTTP请求 */
				nret = ProcessHttpRequest( p_env , p_http_session , p_http_session->p_virtual_host->wwwroot , p_uri , uri_len ) ;
				if( nret == HTTP_OK )
				{
					DebugLog( __FILE__ , __LINE__ , "ProcessHttpRequest ok" );
				}
				else if( nret > 0 )
				{
					/* 格式化响应头和体，用出错状态码 */
					nret = FormatHttpResponseStartLine( nret , p_http_session->http , 1 , NULL ) ;
					if( nret )
					{
						ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , ERRNO );
						return 1;
					}
				}
				else if( nret == 0 )
				{
					DebugLog( __FILE__ , __LINE__ , "ProcessHttpRequest forward" );
					return 0;
				}
				else
				{
					ErrorLog( __FILE__ , __LINE__ , "ProcessHttpRequest failed[%d]" , nret );
					return nret;
				}
			}
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "QueryVirtualHostHashNode[%.*s] not found" , host_len , host );
			
			/* 格式化响应头和体，用出错状态码 */
			nret = FormatHttpResponseStartLine( HTTP_FORBIDDEN , p_http_session->http , 1 , NULL ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , ERRNO );
				return 1;
			}
		}
		
		DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(response_buf,NULL) , GetHttpBufferLength(response_buf) , "HttpResponseBuffer [%d]bytes" , GetHttpBufferLength(response_buf) );
		append_buf = GetHttpAppendBuffer(p_http_session->http) ;
		if( append_buf )
		{
			DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(append_buf,NULL) , GetHttpBufferLength(append_buf) , "HttpAppendBuffer [%d]bytes" , GetHttpBufferLength(append_buf) );
		}
		
#if ( defined __linux ) || ( defined __unix )
		/* 注册epoll写事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLOUT | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
			return -1;
		}
#elif ( defined _WIN32 )
		p_http_session->flag = HTTPSESSION_FLAGS_SENDING ;
		
		/* 投递发送事件 */
		if( p_http_session->ssl == NULL )
		{
			response_buf = GetHttpResponseBuffer( p_http_session->http );
			buf.buf = GetHttpBufferBase( response_buf , NULL ) ;
			buf.len = GetHttpBufferLength( response_buf ) ;
		}
		else
		{
			response_buf = GetHttpResponseBuffer( p_http_session->http );
			SSL_write( p_http_session->ssl , GetHttpBufferBase( response_buf , NULL ) , GetHttpBufferLength( response_buf ) );
			append_buf = GetHttpAppendBuffer( p_http_session->http );
			SSL_write( p_http_session->ssl , GetHttpBufferBase( append_buf , NULL ) , GetHttpBufferLength( append_buf ) );
			buf.buf = p_http_session->out_bio_buffer ;
			buf.len = BIO_read( p_http_session->out_bio , p_http_session->out_bio_buffer , sizeof(p_http_session->out_bio_buffer)-1 ) ;
		}
		dwFlags = 0 ;
		nret = WSASend( p_http_session->netaddr.sock , & buf , 1 , NULL , dwFlags , & (p_http_session->overlapped) , NULL ) ;
		if( nret == SOCKET_ERROR )
		{
			if( WSAGetLastError() == ERROR_IO_PENDING )
			{
				DebugLog( __FILE__ , __LINE__ , "WSASend io pending" );
			}
			else
			{
				ErrorLog( __FILE__ , __LINE__ , "WSASend failed , errno[%d]" , ERRNO );
				return 1;
			}
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "WSASend ok" );
		}
#endif
		
		UpdateHttpSessionTimeoutTreeNode( p_env , p_http_session , (int)GETSECONDSTAMP + p_env->http_options__timeout );
		UpdateHttpSessionElapseTreeNode( p_env , p_http_session , (int)GETSECONDSTAMP + p_env->http_options__elapse );
	}
	
	return 0;
}

