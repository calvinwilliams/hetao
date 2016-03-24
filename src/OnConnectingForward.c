/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int SelectForwardAddress( struct HetaoServer *p_server , struct HttpSession *p_http_session )
{
	struct list_head	*p_curr = NULL , *p_next = NULL ;
	struct ForwardServer	*p_forward_server = NULL ;
	
	list_for_each_safe( p_curr , p_next , & (p_http_session->p_virtualhost->roundrobin_list.roundrobin_node) )
	{
		p_forward_server = container_of( p_curr , struct ForwardServer , roundrobin_node ) ;
		
		if( p_forward_server->timestamp_to_valid == 0 || p_forward_server->timestamp_to_valid > GETSECONDSTAMP )
		{
			list_move_tail( p_curr , & (p_http_session->p_virtualhost->roundrobin_list.roundrobin_node) );
			p_http_session->p_forward_server = p_forward_server ;
			return 0;
		}
	}
	
	return HTTP_SERVICE_UNAVAILABLE;
}

int ConnectForwardServer( struct HetaoServer *p_server , struct HttpSession *p_http_session )
{
	struct epoll_event	event ;
	
	char			*request_base = NULL ;
	int			request_len ;
	struct HttpBuffer	*forward_b = NULL ;
	
	int			nret = 0 ;
	
	p_http_session->connected_flag = 0 ;
	
	/* 创建转发连接 */
	p_http_session->forward_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_http_session->forward_sock == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "socket failed , errno[%d]" , errno );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	SetHttpNonblock( p_http_session->forward_sock );
	SetHttpNodelay( p_http_session->forward_sock , p_server->p_config->tcp_options.nodelay );
	SetHttpNoLinger( p_http_session->forward_sock , p_server->p_config->tcp_options.nolinger );
	
	/* 连接服务端 */
	nret = connect( p_http_session->forward_sock , (struct sockaddr *) & (p_http_session->netaddr.addr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		if( errno == EINPROGRESS )
		{
			DebugLog( __FILE__ , __LINE__ , "connect[%s:%d] inprocess" , p_http_session->netaddr.ip , p_http_session->netaddr.port );
			
			/* 注册epoll写事件 */
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLOUT | EPOLLERR ;
			event.data.ptr = p_http_session ;
			nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->forward_sock , & event ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , errno );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__ , "connect[%s:%d] failed , errno[%d]" , p_http_session->netaddr.ip , p_http_session->netaddr.port , errno );
			return HTTP_SERVICE_UNAVAILABLE;
		}
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "connect[%s:%d] ok" , p_http_session->netaddr.ip , p_http_session->netaddr.port );
		
		/* 复制HTTP请求 */
		request_base = GetHttpBufferBase( GetHttpRequestBuffer(p_http_session->http) , & request_len ) ;
		forward_b = GetHttpResponseBuffer( p_http_session->forward_http ) ;
		nret = MemcatHttpBuffer( forward_b , request_base , request_len ) ;
		if( nret )
		{
			close( p_http_session->forward_sock );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		
		/* 注册epoll写事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLOUT | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->forward_sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , errno );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		
		p_http_session->connected_flag = 1 ;
	}
	
	return 0;
}

int OnConnectingForward( struct HetaoServer *p_server , struct HttpSession *p_http_session )
{
#if ( defined __linux ) || ( defined __unix )
	int			error , code ;
#endif
	_SOCKLEN_T		addr_len ;

	struct HttpBuffer	*b = NULL ;
	
	struct epoll_event	event ;
	
	char			*request_base = NULL ;
	int			request_len ;
	struct HttpBuffer	*forward_b = NULL ;
	
	int			nret = 0 ;
	
	/* 检查非堵塞连接结果 */
#if ( defined __linux ) || ( defined __unix )
	addr_len = sizeof(int) ;
	code = getsockopt( p_http_session->forward_sock , SOL_SOCKET , SO_ERROR , & error , & addr_len ) ;
	if( code < 0 || error )
#elif ( defined _WIN32 )
	addr_len = sizeof(struct sockaddr_in) ;
	nret = connect( p_http_session->forward_sock , (struct sockaddr *) & (p_http_session->netaddr.sockaddr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 && errno == EISCONN )
	{
		;
	}
	else
#endif
        {
		ErrorLog( __FILE__ , __LINE__ , "connect[%s:%d] failed , errno[%d]" , p_http_session->netaddr.ip , p_http_session->netaddr.port , errno );
		
		epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_http_session->forward_sock , NULL );
		close( p_http_session->forward_sock );
		p_http_session->forward_flag = 0 ;
		
		/* 选择转发服务端 */
		nret = SelectForwardAddress( p_server , p_http_session ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "SelectForwardAddress failed[%d] , errno[%d]" , nret , errno );
			
			/* 格式化响应头和体，用出错状态码 */
			nret = FormatHttpResponseStartLine( nret , p_http_session->http , 1 ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , errno );
				return 1;
			}
		}
		else
		{
			/* 连接转发服务端 */
			nret = ConnectForwardServer( p_server , p_http_session ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "SelectForwardAddress failed[%d] , errno[%d]" , nret , errno );
				
				/* 格式化响应头和体，用出错状态码 */
				nret = FormatHttpResponseStartLine( nret , p_http_session->http , 1 ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , errno );
					return 1;
				}
			}
			else
			{
				/* 暂禁原连接事件 */
				p_http_session->forward_flag = 1 ;
				return 0;
			}
		}
		
		b = GetHttpResponseBuffer(p_http_session->http) ;
		DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "HttpResponseBuffer" );
		
		/* 注册epoll写事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLOUT | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , errno );
			return -1;
		}
		
		return 0;
        }
	
	DebugLog( __FILE__ , __LINE__ , "connect2[%s:%d] ok" , p_http_session->netaddr.ip , p_http_session->netaddr.port );
	
	/* 复制HTTP请求 */
	request_base = GetHttpBufferBase( GetHttpRequestBuffer(p_http_session->http) , & request_len ) ;
	forward_b = GetHttpResponseBuffer( p_http_session->forward_http ) ;
	nret = MemcatHttpBuffer( forward_b , request_base , request_len ) ;
	if( nret )
	{
		close( p_http_session->forward_sock );
		return 1;
	}
	
	/* 注册epoll写事件 */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLOUT | EPOLLERR ;
	event.data.ptr = p_http_session ;
	nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->forward_sock , & event ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , errno );
		return 1;
	}
	
	p_http_session->connected_flag = 1 ;
	
	return 0;
}
