/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int OnSendingForward( struct HetaoServer *p_server , struct HttpSession *p_http_session )
{
	struct epoll_event	event ;
	
	int			nret = 0 ;
	
	/* 发一把HTTP请求 */
	nret = SendHttpRequestNonblock( p_http_session->forward_sock , NULL , p_http_session->forward_http ) ;
	if( nret == FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
	{
		/* 没发完 */
		DebugLog( __FILE__ , __LINE__ , "SendHttpRequestNonblock return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK" );
	}
	else if( nret )
	{
		/* 发送报错了 */
		ErrorLog( __FILE__ , __LINE__ , "SendHttpRequestNonblock failed[%d] , errno[%d]" , nret , errno );
		return 1;
	}
	else
	{
		/* 发送完HTTP请求了 */
		DebugLog( __FILE__ , __LINE__ , "SendHttpRequestNonblock done" );
		
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLIN | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->forward_sock , & event ) ;
		if( nret == 1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , errno );
			return -1;
		}
	}
	
	return 0;
}
