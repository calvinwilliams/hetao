/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int InitVirtualHostHash( struct ListenSession *p_listen_session , int count )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	
	p_listen_session->virtual_host_hashsize = count * 2 ;
	p_listen_session->virtual_host_hash = (struct hlist_head *)malloc( sizeof(struct hlist_head) * p_listen_session->virtual_host_hashsize ) ;
	if( p_listen_session->virtual_host_hash == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		return -1;
	}
	memset( p_listen_session->virtual_host_hash , 0x00 , sizeof(struct hlist_head) * p_listen_session->virtual_host_hashsize );
	
	for( i = 0 , p_hlist_head = p_listen_session->virtual_host_hash ; i < p_listen_session->virtual_host_hashsize ; i++ , p_hlist_head++ )
	{
		INIT_HLIST_HEAD( p_hlist_head );
	}
	
	return 0;
}

void CleanVirtualHostHash( struct ListenSession *p_listen_session )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct hlist_node	*curr = NULL , *next = NULL ;
	struct VirtualHost	*p_virtual_host = NULL ;
	struct RewriteUri	*p_rewrite_uri = NULL ;
	struct RewriteUri	*p_next_rewrite_uri = NULL ;
	
	for( i = 0 , p_hlist_head = p_listen_session->virtual_host_hash ; i < p_listen_session->virtual_host_hashsize ; i++ , p_hlist_head++ )
	{
		hlist_for_each_safe( curr , next , p_hlist_head )
		{
			hlist_del( curr );
			p_virtual_host = container_of(curr,struct VirtualHost,virtual_host_node) ;
			list_for_each_entry_safe( p_rewrite_uri , p_next_rewrite_uri , & (p_virtual_host->rewrite_uri_list.rewrite_uri_node) , struct RewriteUri , rewrite_uri_node )
			{
				free( p_rewrite_uri->pattern_re );
			}
			if( p_virtual_host->forward_ssl_ctx )
			{
				SSL_CTX_free( p_virtual_host->forward_ssl_ctx );
				p_virtual_host->forward_ssl_ctx = NULL ;
			}
			CleanRedirectDomainHash( p_virtual_host );
#if ( defined _WIN32 )
			CloseHandle( p_virtual_host->directory_changes_handler );
#endif
			free( p_virtual_host );
		}
	}
	
	free( p_listen_session->virtual_host_hash );
	p_listen_session->virtual_host_hash = NULL ;
	
	return;
}

int PushVirtualHostHashNode( struct ListenSession *p_listen_session , struct VirtualHost *p_virtual_host )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct VirtualHost	*p ;
	
	index = CalcHash(p_virtual_host->domain,p_virtual_host->domain_len) % (p_listen_session->virtual_host_hashsize) ;
	/* DebugLog( __FILE__ , __LINE__ , "VirtualHost[%d]=CalcHash[%.*s][%d]" , index , p_virtual_host->domain_len , p_virtual_host->domain , p_listen_session->virtual_host_hashsize ); */
	p_hlist_head = p_listen_session->virtual_host_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , struct VirtualHost , virtual_host_node )
	{
		if( STRCMP( p->domain , == , p_virtual_host->domain ) )
			return 1;
	}
	hlist_add_head( & (p_virtual_host->virtual_host_node) , p_hlist_head );
	
	return 0;
}

struct VirtualHost *QueryVirtualHostHashNode( struct ListenSession *p_listen_session , char *domain , int domain_len )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct VirtualHost	*p ;
	
	index = CalcHash(domain,domain_len) % (p_listen_session->virtual_host_hashsize) ;
	p_hlist_head = p_listen_session->virtual_host_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , struct VirtualHost , virtual_host_node )
	{
		if( p->domain_len == domain_len && STRNCMP( p->domain , == , domain , domain_len ) )
			return p;
	}
	
	return NULL;
}

