/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

static signed char		g_exit_flag = 0 ;
signed char			g_second_elapse = 0 ;

void *WorkerThread( void *pv )
{
	struct HetaoServer		*p_server = (struct HetaoServer *)pv ;
	
	struct epoll_event		event , *p_event = NULL ;
	struct epoll_event		events[ MAX_EPOLL_EVENTS ] ;
	int				i , j ;
	
	struct DataSession		*p_data_session = NULL ;
	struct list_head		*p_curr = NULL , *p_next = NULL ;
	struct ListenSession		*p_listen_session = NULL ;
	struct HtmlCacheSession		*p_htmlcache_session = NULL ;
	struct HttpSession		*p_http_session = NULL ;
	
	int				nret = 0 ;
	
	InfoLog( __FILE__ , __LINE__ , "--- worker[%d] begin ---" , p_server->process_info_index );
	
	/* 如果需要吧绑定CPU，绑定之 */
	if( p_server->p_config->cpu_affinity )
	{
		BindCpuAffinity( p_server->process_info_index );
	}
	
	/* 创建多路复用池 */
	p_server->p_this_process_info->epoll_fd = epoll_create( 1024 ) ;
	if( p_server->p_this_process_info->epoll_fd == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "[%d]epoll_create failed , errno[%d]" , p_server->process_info_index , errno );
		return NULL;
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "[%d]epoll_create ok #%d#" , p_server->process_info_index , p_server->p_this_process_info->epoll_fd );
	}
	SetHttpCloseExec( p_server->p_this_process_info->epoll_fd );
	
	/* 注册管道事件 */
	p_server->pipe_session.type = DATASESSION_TYPE_PIPE ;
	
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = & (p_server->pipe_session) ;
	nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_ADD , p_server->p_this_process_info->pipe[0] , & event ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "epoll_ctl #%d# failed , errno[%d]" , p_server->p_this_process_info->epoll_fd , errno );
		return NULL;
	}
	
	/* 注册网页缓存事件 */
	p_server->htmlcache_session.type = DATASESSION_TYPE_HTMLCACHE ;
	
	p_server->htmlcache_inotify_fd = inotify_init() ;
	if( p_server->htmlcache_inotify_fd == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "inotify_init failed , errno[%d]" , errno );
		return NULL;
	}
	SetHttpCloseExec( p_server->htmlcache_inotify_fd );
	
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = & (p_server->htmlcache_session) ;
	nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_ADD , p_server->htmlcache_inotify_fd , & event ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , errno );
		return NULL;
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "epoll_ctl #%d# add inotify_fd #%d#" , p_server->p_this_process_info->epoll_fd , p_server->htmlcache_inotify_fd );
	}
	
	/* 如果配置了ACCEPT锁，或者是第一个工作进程，注册侦听端口事件 */
	if( p_server->p_config->accept_mutex == 0 || p_server->p_this_process_info == p_server->process_info_array )
	{
		list_for_each( p_curr , & (p_server->listen_session_list.list) )
		{
			p_listen_session = container_of( p_curr , struct ListenSession , list ) ;
			
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_listen_session ;
			nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_ADD , p_listen_session->netaddr.sock , & event ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "epoll_ctl add listen failed , errno[%d]" , errno );
				return NULL;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "epoll_ctl #%d# add listen #%d#" , p_server->p_this_process_info->epoll_fd , p_listen_session->netaddr.sock );
			}
		}
	}
	
	/* 主工作循环 */
	while( g_exit_flag == 0 || p_server->http_session_used_count > 0 )
	{
		/* 等待epoll事件 */
		InfoLog( __FILE__ , __LINE__ , "[%d]epoll_wait #%d# ... [%d][%d][%d,%d]" , p_server->process_info_index , p_server->p_this_process_info->epoll_fd , p_server->listen_session_count , p_server->htmlcache_session_count , p_server->http_session_used_count , p_server->http_session_unused_count );
		memset( events , 0x00 , sizeof(events) );
		p_server->p_this_process_info->epoll_nfds = epoll_wait( p_server->p_this_process_info->epoll_fd , events , MAX_EPOLL_EVENTS , 1000 ) ;
		if( p_server->p_this_process_info->epoll_nfds == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "[%d]epoll_wait failed , errno[%d]" , p_server->process_info_index , errno );
			return NULL;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "[%d]epoll_wait #%d# return[%d]events" , p_server->process_info_index , p_server->p_this_process_info->epoll_fd , p_server->p_this_process_info->epoll_nfds );
		}
		
		/* 定时器线程点亮了秒漏事件，检查超时的HTTP通讯会话，关闭之 */
		if( g_second_elapse == 1 )
		{
			struct HttpSession	*p_http_session = NULL ;
			
			while(1)
			{
				p_http_session = GetExpireHttpSessionTimeoutTreeNode( p_server ) ;
				if( p_http_session == NULL )
					break;
				
				ErrorLog( __FILE__ , __LINE__ , "SESSION TIMEOUT --------- client_ip[%s]" , p_http_session->netaddr.ip );
				SetHttpSessionUnused( p_server , p_http_session );
			}
			
			g_second_elapse = 0 ;
		}
		
		/* 处理所有事件 */
		for( i = 0 , p_event = events ; i < p_server->p_this_process_info->epoll_nfds ; i++ , p_event++ )
		{
			p_data_session = p_event->data.ptr ;
			
			/* 如果是侦听端口事件 */
			if( p_data_session->type == DATASESSION_TYPE_LISTEN )
			{
				p_listen_session = (struct ListenSession *)p_data_session ;
				
				DebugLog( __FILE__ , __LINE__ , "[%d]DATASESSION_TYPE_LISTEN[%p] ---------" , p_server->process_info_index , p_data_session );
				
				if( p_event->events & EPOLLIN )
				{
					/* 有新的连接进来 */
					struct ProcessInfo	*p_process_info = NULL ;
					struct ProcessInfo	*p_process_info_with_min_balance = NULL ;
					
					DebugLog( __FILE__ , __LINE__ , "EPOLLIN" );
					
					nret = OnAcceptingSocket( p_server , p_listen_session ) ;
					if( nret > 0 )
					{
						WarnLog( __FILE__ , __LINE__ , "OnAcceptingSocket warn[%d] , errno[%d]" , nret , errno );
					}
					else if( nret < 0 )
					{
						ErrorLog( __FILE__ , __LINE__ , "OnAcceptingSocket failed[%d] , errno[%d]" , nret , errno );
						return NULL;
					}
					else
					{
						DebugLog( __FILE__ , __LINE__ , "OnAcceptingSocket ok" );
					}
					
					/* 如果打开了ACCEPT锁，或者工作进程数量大于一个 */
					if( p_server->p_config->accept_mutex == 1 && p_server->p_config->worker_processes > 1 )
					{
						/* 选择最近处理事件最少的工作进程 */
						p_process_info_with_min_balance = p_server->process_info_array ;
						DebugLog( __FILE__ , __LINE__ , "process[%d] epoll_nfds[%d]" , 0 , p_process_info_with_min_balance->epoll_nfds );
						for( j = 1 , p_process_info = p_server->process_info_array + 1 ; j < p_server->p_config->worker_processes ; j++ , p_process_info++ )
						{
							DebugLog( __FILE__ , __LINE__ , "process[%d] epoll_nfds[%d]" , j , p_process_info->epoll_nfds );
							if( p_process_info->epoll_nfds < p_process_info_with_min_balance->epoll_nfds )
								p_process_info_with_min_balance = p_process_info ;
						}
						DebugLog( __FILE__ , __LINE__ , "p_process_info_with_min_balance process[%d]" , p_process_info_with_min_balance-p_server->process_info_array );
						
						if( p_process_info_with_min_balance != p_server->p_this_process_info )
						{
							/* 后一轮接受新连接的任务就交给你了 */
							list_for_each( p_curr , & (p_server->listen_session_list.list) )
							{
								p_listen_session = container_of( p_curr , struct ListenSession , list ) ;
								
								nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_listen_session->netaddr.sock , NULL ) ;
								if( nret == -1 )
								{
									ErrorLog( __FILE__ , __LINE__ , "[%d]epoll_ctl failed , errno[%d]" , p_server->process_info_index , errno );
									return NULL;
								}
								else
								{
									DebugLog( __FILE__ , __LINE__ , "[%d]epoll_ctl #%d# del listen #%d#" , p_server->process_info_index , p_server->p_this_process_info->epoll_fd , p_listen_session->netaddr.sock );
								}
								
								memset( & event , 0x00 , sizeof(struct epoll_event) );
								event.events = EPOLLIN | EPOLLERR ;
								event.data.ptr = p_listen_session ;
								nret = epoll_ctl( p_process_info_with_min_balance->epoll_fd , EPOLL_CTL_ADD , p_listen_session->netaddr.sock , & event ) ;
								if( nret == -1 )
								{
									ErrorLog( __FILE__ , __LINE__ , "[%d]epoll_ctl failed , errno[%d]" , p_process_info_with_min_balance-p_server->process_info_array , errno );
									return NULL;
								}
								else
								{
									DebugLog( __FILE__ , __LINE__ , "[%d]epoll_ctl #%d# add listen #%d#" , p_process_info_with_min_balance-p_server->process_info_array , p_process_info_with_min_balance->epoll_fd , p_listen_session->netaddr.sock );
								}
							}
						}
					}
				}
				else if( p_event->events & EPOLLERR )
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLLERR" );
					
					ErrorLog( __FILE__ , __LINE__ , "listen_sock epoll EPOLLERR" );
					return NULL;
				}
				else
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLL?" );
					
					ErrorLog( __FILE__ , __LINE__ , "listen_sock epoll event invalid[%d]" , p_event->events );
					return NULL;
				}
			}
			/* 如果是HTTP通讯事件 */
			else if( p_data_session->type == DATASESSION_TYPE_HTTP )
			{
				p_http_session = (struct HttpSession *)p_data_session ;
				
				DebugLog( __FILE__ , __LINE__ , "[%d]DATASESSION_TYPE_HTTP[%p] ---------" , p_server->process_info_index , p_data_session );
				
				if( p_event->events & EPOLLIN )
				{
					/* HTTP接收事件。如果接收完成，则马上处理 */
					DebugLog( __FILE__ , __LINE__ , "EPOLLIN" );
					
					if( p_http_session->forward_flags == 0 )
					{
						nret = OnReceivingSocket( p_server , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnReceivingSocket done[%d]" , nret );
							SetHttpSessionUnused( p_server , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnReceivingSocket failed[%d] , errno[%d]" , nret , errno );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnReceivingSocket ok" );
						}
					}
					else
					{
						nret = OnReceivingForward( p_server , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnReceivingForward done[%d]" , nret );
							SetHttpSessionUnused( p_server , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnReceivingForward failed[%d] , errno[%d]" , nret , errno );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnReceivingForward ok" );
						}
					}
				}
				else if( p_event->events & EPOLLOUT )
				{
					/* HTTP发送事件。如果设置了Keep-Alive则等待下一个HTTP请求 */
					DebugLog( __FILE__ , __LINE__ , "EPOLLOUT" );
					
					if( p_http_session->forward_flags == 0 )
					{
						nret = OnSendingSocket( p_server , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnSendingSocket done[%d]" , nret );
							SetHttpSessionUnused( p_server , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnSendingSocket failed[%d] , errno[%d]" , nret , errno );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnSendingSocket ok" );
						}
					}
					else
					{
						if( p_http_session->forward_flags & HTTPSESSION_FLAGS_CONNECTED )
						{
							nret = OnSendingForward( p_server , p_http_session ) ;
							if( nret > 0 )
							{
								DebugLog( __FILE__ , __LINE__ , "OnSendingForward done[%d]" , nret );
								SetHttpSessionUnused( p_server , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnSendingForward failed[%d] , errno[%d]" , nret , errno );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnSendingForward ok" );
							}
						}
						else if( p_http_session->forward_flags & HTTPSESSION_FLAGS_CONNECTING )
						{
							nret = OnConnectingForward( p_server , p_http_session ) ;
							if( nret > 0 )
							{
								DebugLog( __FILE__ , __LINE__ , "OnConnectingForward done[%d]" , nret );
								SetHttpSessionUnused( p_server , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnConnectingForward failed[%d] , errno[%d]" , nret , errno );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnConnectingForward ok" );
							}
						}
					}
				}
				else if( p_event->events & EPOLLRDHUP )
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLLRDHUP" );
					
					if( p_http_session->forward_flags )
					{
						InfoLog( __FILE__ , __LINE__ , "http sock epoll EPOLLRDHUP" );
						SetHttpSessionUnused( p_server , p_http_session );
					}
				}
				else if( p_event->events & EPOLLERR )
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLLERR" );
					
					if( p_http_session->forward_flags == 0 )
					{
						ErrorLog( __FILE__ , __LINE__ , "http sock epoll EPOLLERR" );
						SetHttpSessionUnused( p_server , p_http_session );
					}
					else
					{
						if( p_http_session->forward_flags & HTTPSESSION_FLAGS_CONNECTING )
						{
							nret = OnConnectingForward( p_server , p_http_session ) ;
							if( nret > 0 )
							{
								DebugLog( __FILE__ , __LINE__ , "OnConnectingForward done[%d]" , nret );
								SetHttpSessionUnused( p_server , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnConnectingForward failed[%d] , errno[%d]" , nret , errno );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnConnectingForward ok" );
							}
						}
					}
				}
				else
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLL?" );
					
					ErrorLog( __FILE__ , __LINE__ , "http sock epoll event invalid[%d]" , p_event->events );
					SetHttpSessionUnused( p_server , p_http_session );
				}
			}
			/* 如果是网页缓存事件 */
			else if( p_data_session->type == DATASESSION_TYPE_HTMLCACHE )
			{
				p_htmlcache_session = (struct HtmlCacheSession *)p_data_session ;
				
				DebugLog( __FILE__ , __LINE__ , "[%d]DATASESSION_TYPE_HTMLCACHE[%p] ---------" , p_server->process_info_index , p_data_session );
				
				nret = HtmlCacheEventHander( p_server ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "HtmlCacheEventHander failed[%d] , errno[%d]" , nret , errno );
					return NULL;
				}
				else
				{
					DebugLog( __FILE__ , __LINE__ , "HtmlCacheEventHander ok" );
				}
			}
			/* 如果是管道事件 */
			else if( p_data_session->type == DATASESSION_TYPE_PIPE )
			{
				int	n ;
				char	ch = 0 ;
				
				DebugLog( __FILE__ , __LINE__ , "[%d]DATASESSION_TYPE_PIPE[%p] ---------" , p_server->process_info_index , p_data_session );
				
				n = read( p_server->p_this_process_info->pipe[0] , & ch , 1 ) ;
				InfoLog( __FILE__ , __LINE__ , "read pipe return[%d] , ch[%c]" , n , ch );
				if( n == 0 )
				{
					/* 管理进程发送给了优雅结束信号 */
					struct epoll_event	*p_clean_event = NULL ;
					struct ListenSession	*p_clean_session = NULL ;
					
					/* 关闭所有侦听 */
					list_for_each_safe( p_curr , p_next , & (p_server->listen_session_list.list) )
					{
						p_clean_session = list_entry( p_curr , struct ListenSession , list ) ;
						epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_clean_session->netaddr.sock , NULL );
						close( p_clean_session->netaddr.sock );
					}
					
					/* 屏蔽所有后续侦听事件 */
					for( j = p_event-events+1 , p_clean_event = events+j ; j < p_server->p_this_process_info->epoll_nfds ; j++ , p_clean_event++ )
					{
						p_clean_session = p_clean_event->data.ptr ;
						if( p_clean_session->type == DATASESSION_TYPE_LISTEN )
							p_clean_session->type = 0 ;
					}
					
					/* 关闭管道 */
					epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_server->p_this_process_info->pipe[0] , NULL );
					close( p_server->p_this_process_info->pipe[0] );
					
					g_exit_flag = 1 ;
				}
				else if( n > 0 )
				{
					if( ch == SIGNAL_REOPEN_LOG )
					{
						struct VirtualHost	*p_virtualhost = NULL ;
						
						/* 管理进程发送给了重新打开日志信号 */
						CloseLogFile();
						
						for( i = 0 ; i < p_server->virtualhost_hashsize ; i++ )
						{
							hlist_for_each_entry( p_virtualhost , p_server->virtualhost_hash+i , virtualhost_node )
							{
								DebugLog( __FILE__ , __LINE__ , "close access_log[%s] #%d#" , p_virtualhost->access_log , p_virtualhost->access_log_fd );
								close( p_virtualhost->access_log_fd );
								
								p_virtualhost->access_log_fd = OPEN( p_virtualhost->access_log , O_CREAT_WRONLY_APPEND ) ;
								if( p_virtualhost->access_log_fd == -1 )
								{
									ErrorLog( __FILE__ , __LINE__ ,  "open access log[%s] failed , errno[%d]" , p_virtualhost->access_log , errno );
									return NULL;
								}
								else
								{
									DebugLog( __FILE__ , __LINE__ ,  "open access log[%s] ok" , p_virtualhost->access_log );
								}
								
							}
						}
					}
				}
				else
				{
					ErrorLog( __FILE__ , __LINE__ ,  "read pipe failed , errno[%d]" , errno );
					return NULL;
				}
			}
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "--- worker[%d] exit ---" , p_server->process_info_index );
	
	return NULL;
}

