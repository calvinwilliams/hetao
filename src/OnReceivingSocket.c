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
	struct epoll_event	event ;
	
	int			nret = 0 ;
	
	/* 收一把HTTP请求 */
	nret = ReceiveHttpRequestNonblock( p_http_session->netaddr.sock , p_http_session->ssl , p_http_session->http ) ;
	if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
	{
		/* 没收完整 */
		DebugLog( __FILE__ , __LINE__ , "ReceiveHttpRequestNonblock return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER" );
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
			ErrorLog( __FILE__ , __LINE__ , "ReceiveHttpRequestNonblock failed[%d] , errno[%d]" , nret , errno );
			return 1;
		}
	}
	else
	{
		/* 收满一个HTTP请求 */
		char			*host = NULL ;
		int			host_len ;
		
		struct RewriteUrl	*p_rewrite_url = NULL ;
		char			*p_url = NULL ;
		char			url[ 4096 + 1 ] ;
		int			url_len ;
		
		struct HttpBuffer	*b = NULL ;
		
		DebugLog( __FILE__ , __LINE__ , "ReceiveHttpRequestNonblock done" );
		
		UpdateHttpSessionTimeoutTreeNode( p_env , p_http_session , GETSECONDSTAMP + p_env->http_options__timeout );
		
		b = GetHttpRequestBuffer(p_http_session->http) ;
		DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "HttpRequestBuffer [%d]bytes" , GetHttpBufferLength(b) );
		
		/* 查询虚拟主机 */
		host = QueryHttpHeaderPtr( p_http_session->http , "Host" , & host_len ) ;
		if( host == NULL )
			host = "" , host_len = 0 ;
		p_http_session->p_virtualhost = QueryVirtualHostHashNode( p_http_session->p_listen_session , host , host_len ) ;
		if( p_http_session->p_virtualhost == NULL && p_http_session->p_listen_session->p_virtualhost_default )
		{
			p_http_session->p_virtualhost = p_http_session->p_listen_session->p_virtualhost_default ;
		}
		
		/* 处理HTTP请求 */
		if( p_http_session->p_virtualhost )
		{
			DebugLog( __FILE__ , __LINE__ , "QueryVirtualHostHashNode[%.*s] ok , wwwroot[%s]" , host_len , host , p_http_session->p_virtualhost->wwwroot );
			
			/* REWRITE */
			if( p_env->template_re == NULL )
			{
				p_url = GetHttpHeaderPtr_URI(p_http_session->http,NULL) ;
				url_len = GetHttpHeaderLen_URI(p_http_session->http) ;
			}
			else
			{
				p_url = NULL ;
				list_for_each_entry( p_rewrite_url , & (p_http_session->p_virtualhost->rewrite_url_list.rewriteurl_node) , rewriteurl_node )
				{
					strcpy( url , p_rewrite_url->template );
					url_len = p_rewrite_url->template_len ;
					
					nret = RegexReplaceString( p_rewrite_url->pattern_re , GetHttpHeaderPtr_URI(p_http_session->http,NULL) , GetHttpHeaderLen_URI(p_http_session->http) , p_env->template_re , url , & url_len , sizeof(url) ) ;
					if( nret == 0 )
					{
						DebugLog( __FILE__ , __LINE__ , "RegexReplaceString[%.*s][%s][%s] ok[%.*s]" , GetHttpHeaderLen_URI(p_http_session->http) , GetHttpHeaderPtr_URI(p_http_session->http,NULL) , p_rewrite_url->pattern , p_rewrite_url->template , url_len , url );
						p_url = url ;
						break;
					}
					else if( nret == -1 )
					{
						ErrorLog( __FILE__ , __LINE__ , "RegexReplaceString[%.*s][%s][%s] failed[%d] , errno[%d]" , GetHttpHeaderLen_URI(p_http_session->http) , GetHttpHeaderPtr_URI(p_http_session->http,NULL) , p_rewrite_url->pattern , p_rewrite_url->template , nret , errno );
						return HTTP_BAD_REQUEST;
					}
					else
					{
						DebugLog( __FILE__ , __LINE__ , "RegexReplaceString[%.*s][%s][%s] continue" , GetHttpHeaderLen_URI(p_http_session->http) , GetHttpHeaderPtr_URI(p_http_session->http,NULL) , p_rewrite_url->pattern , p_rewrite_url->template );
					}
				}
				if( p_url == NULL )
				{
					p_url = GetHttpHeaderPtr_URI(p_http_session->http,NULL) ;
					url_len = GetHttpHeaderLen_URI(p_http_session->http) ;
				}
			}
			
			/* 处理HTTP请求 */
			nret = ProcessHttpRequest( p_env , p_http_session , p_http_session->p_virtualhost->wwwroot , p_url , url_len ) ;
			if( nret == HTTP_OK )
			{
				DebugLog( __FILE__ , __LINE__ , "ProcessHttpRequest ok" );
			}
			else if( nret > 0 )
			{
				/* 格式化响应头和体，用出错状态码 */
				nret = FormatHttpResponseStartLine( nret , p_http_session->http , 1 ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , errno );
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
		else
		{
			DebugLog( __FILE__ , __LINE__ , "QueryVirtualHostHashNode[%.*s] not found" , host_len , host );
			
			/* 格式化响应头和体，用出错状态码 */
			nret = FormatHttpResponseStartLine( HTTP_FORBIDDEN , p_http_session->http , 1 ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , errno );
				return 1;
			}
		}
		
		b = GetHttpResponseBuffer(p_http_session->http) ;
		DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "HttpResponseBuffer [%d]bytes" , GetHttpBufferLength(b) );
		b = GetHttpAppendBuffer(p_http_session->http) ;
		if( b )
		{
			DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "HttpResponseBuffer [%d]bytes" , GetHttpBufferLength(b) );
		}
		
		/* 注册epoll写事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLOUT | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , errno );
			return -1;
		}
		
		/* 直接来一发 */
		/*
		if( p_env->p_config->worker_processes == 1 )
		{
			nret = OnSendingSocket( p_env , p_http_session ) ;
			if( nret > 0 )
			{
				DebugLog( __FILE__ , __LINE__ , "OnSendingSocket done[%d]" , nret );
				return nret;
			}
			else if( nret < 0 )
			{
				ErrorLog( __FILE__ , __LINE__ , "OnSendingSocket failed[%d] , errno[%d]" , nret , errno );
				return nret;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "OnSendingSocket ok" );
			}
		}
		*/
	}
	
	return 0;
}

