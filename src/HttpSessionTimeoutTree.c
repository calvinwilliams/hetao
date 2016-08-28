/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int AddHttpSessionTimeoutTreeNode( struct HetaoServer *p_server , struct HttpSession *p_http_session )
{
        struct rb_node		**pp_new_node = NULL ;
        struct rb_node		*p_parent = NULL ;
        struct HttpSession	*p = NULL ;
	
	pp_new_node = & (p_server->http_session_rbtree_used.rb_node) ;
        while( *pp_new_node )
        {
                p = container_of( *pp_new_node , struct HttpSession , timeout_rbnode ) ;
		
                p_parent = (*pp_new_node) ;
		
                if( p_http_session->timeout_timestamp < p->timeout_timestamp )
                        pp_new_node = &((*pp_new_node)->rb_left) ;
                else if( p_http_session->timeout_timestamp > p->timeout_timestamp )
                        pp_new_node = &((*pp_new_node)->rb_right) ;
                else 
                        pp_new_node = &((*pp_new_node)->rb_left) ;
        }
	
        rb_link_node( & (p_http_session->timeout_rbnode) , p_parent , pp_new_node );
        rb_insert_color( & (p_http_session->timeout_rbnode) , &(p_server->http_session_rbtree_used) );
	
	return 0;
}

void RemoveHttpSessionTimeoutTreeNode( struct HetaoServer *p_server , struct HttpSession *p_http_session )
{
	rb_erase( & (p_http_session->timeout_rbnode) , &(p_server->http_session_rbtree_used) );
	return;
}

int UpdateHttpSessionTimeoutTreeNode( struct HetaoServer *p_server , struct HttpSession *p_http_session , int timeout_timestamp )
{
	int		nret = 0 ;
	
	RemoveHttpSessionTimeoutTreeNode( p_server , p_http_session );
	
	p_http_session->timeout_timestamp = timeout_timestamp ;
	nret = AddHttpSessionTimeoutTreeNode( p_server , p_http_session ) ;
	
	return nret;
}

struct HttpSession *GetExpireHttpSessionTimeoutTreeNode( struct HetaoServer *p_server )
{
	struct rb_node		*p_curr = NULL ;
	struct HttpSession	*p_http_session = NULL ;
	
	p_curr = rb_first( & (p_server->http_session_rbtree_used) ); 
	if (p_curr == NULL)
		return NULL;
	
	p_http_session = rb_entry( p_curr , struct HttpSession , timeout_rbnode );
	if( p_http_session->timeout_timestamp - GETSECONDSTAMP <= 0 )
		return p_http_session;
	
	return NULL;
}

