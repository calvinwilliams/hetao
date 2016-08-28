/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int AddHtmlCacheWdTreeNode( struct HetaoServer *p_server , struct HtmlCacheSession *p_htmlcache_session )
{
        struct rb_node		**pp_new_node = NULL ;
        struct rb_node		*p_parent = NULL ;
        struct HtmlCacheSession	*p = NULL ;
	
	pp_new_node = & (p_server->htmlcache_wd_rbtree.rb_node) ;
        while( *pp_new_node )
        {
                p = container_of( *pp_new_node , struct HtmlCacheSession , htmlcache_wd_rbnode ) ;
		
                p_parent = (*pp_new_node) ;
		
                if( p_htmlcache_session->wd < p->wd )
                        pp_new_node = &((*pp_new_node)->rb_left) ;
                else if( p_htmlcache_session->wd > p->wd )
                        pp_new_node = &((*pp_new_node)->rb_right) ;
                else 
                        pp_new_node = &((*pp_new_node)->rb_left) ;
        }
	
        rb_link_node( & (p_htmlcache_session->htmlcache_wd_rbnode) , p_parent , pp_new_node );
        rb_insert_color( & (p_htmlcache_session->htmlcache_wd_rbnode) , &(p_server->htmlcache_wd_rbtree) );
	
	return 0;
}

struct HtmlCacheSession *QueryHtmlCacheWdTreeNode( struct HetaoServer *p_server , int wd )
{
	struct rb_node		*node = p_server->htmlcache_wd_rbtree.rb_node ;
	struct HtmlCacheSession	*p = NULL ;

	while( node )
	{
		p = container_of( node , struct HtmlCacheSession , htmlcache_wd_rbnode ) ;
		if( wd < p->wd )
			node = node->rb_left ;
		else if( wd > p->wd )
			node = node->rb_right ;
		else
			return p ;
	}
	
	return NULL;
}

void RemoveHtmlCacheWdTreeNode( struct HetaoServer *p_server , struct HtmlCacheSession *p_htmlcache_session )
{
	rb_erase( & (p_htmlcache_session->htmlcache_wd_rbnode) , & (p_server->htmlcache_wd_rbtree) );
	return;
}

