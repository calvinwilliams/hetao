/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

#if ( defined __linux ) || ( defined __unix )

int OnAcceptingSslSocket( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
	X509			*x509 = NULL ;
	char			*line = NULL ;
	struct epoll_event	event ;
	
	int			err ;
	
	int			nret = 0 ;
	
	nret = SSL_do_handshake( p_http_session->ssl ) ;
	if( nret == 1 )
	{
		x509 = SSL_get_peer_certificate( p_http_session->ssl ) ;
		if( x509 )
		{
			line = X509_NAME_oneline( X509_get_subject_name(x509) , 0 , 0 );
			free( line );
			line = X509_NAME_oneline( X509_get_issuer_name(x509) , 0 , 0 );
			free( line );
			X509_free( x509 );
		}
		
		/* 注册epoll读事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLIN | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl #%d# add #%d# EPOLLIN failed , errno[%d]" , p_env->p_this_process_info->epoll_fd , p_http_session->netaddr.sock , ERRNO );
			return 1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "epoll_ctl #%d# mod #%d# EPOLLIN" , p_env->p_this_process_info->epoll_fd , p_http_session->netaddr.sock );
		}
		
		p_http_session->ssl_connected = 1 ;
		
		return 0;
	}
	
	err = SSL_get_error( p_http_session->ssl , nret ) ;
	if( err == SSL_ERROR_WANT_WRITE )
	{
		/* 注册epoll写事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLOUT | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl #%d# mod #%d# SSL EPOLLOUT failed , errno[%d]" , p_env->p_this_process_info->epoll_fd , p_http_session->netaddr.sock , ERRNO );
			return 1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "epoll_ctl #%d# add #%d# SSL EPOLLOUT" , p_env->p_this_process_info->epoll_fd , p_http_session->netaddr.sock );
		}
		
		return 0;
	}
	else if( err == SSL_ERROR_WANT_READ )
	{
		/* 注册epoll读事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLIN | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl #%d# mod #%d# SSL EPOLLIN failed , errno[%d]" , p_env->p_this_process_info->epoll_fd , p_http_session->netaddr.sock , ERRNO );
			return 1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "epoll_ctl #%d# mod #%d# SSL EPOLLIN" , p_env->p_this_process_info->epoll_fd , p_http_session->netaddr.sock );
		}
		
		return 0;
	}
	else
	{
		ErrorLog( __FILE__ , __LINE__ , "SSL_get_error[%d]" , err );
		return 1;
	}
}

#elif ( defined _WIN32 )


#endif

