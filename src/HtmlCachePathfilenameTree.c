/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int AddHtmlCachePathfilenameTreeNode( struct HetaoServer *p_server , struct HtmlCacheSession *p_htmlcache_session )
{
        struct rb_node		**pp_new_node = NULL ;
        struct rb_node		*p_parent = NULL ;
        struct HtmlCacheSession	*p = NULL ;
	int			result ;
	
	pp_new_node = & (p_server->htmlcache_pathfilename_rbtree.rb_node) ;
        while( *pp_new_node )
        {
                p = container_of( *pp_new_node , struct HtmlCacheSession , htmlcache_pathfilename_rbnode ) ;
		
                p_parent = (*pp_new_node) ;
		
		result = strcmp( p_htmlcache_session->pathfilename , p->pathfilename ) ;
                if( result < 0 )
                        pp_new_node = &((*pp_new_node)->rb_left) ;
                else if( result > 0 )
                        pp_new_node = &((*pp_new_node)->rb_right) ;
                else 
                        return -1;
        }
	
        rb_link_node( & (p_htmlcache_session->htmlcache_pathfilename_rbnode) , p_parent , pp_new_node );
        rb_insert_color( & (p_htmlcache_session->htmlcache_pathfilename_rbnode) , &(p_server->htmlcache_pathfilename_rbtree) );
	
	return 0;
}

struct HtmlCacheSession *QueryHtmlCachePathfilenameTreeNode( struct HetaoServer *p_server , char *pathfilename )
{
	struct rb_node		*node = p_server->htmlcache_pathfilename_rbtree.rb_node ;
	struct HtmlCacheSession	*p = NULL ;
	int			result ;

	while( node )
	{
		p = container_of( node , struct HtmlCacheSession , htmlcache_pathfilename_rbnode ) ;
		
		result = strcmp( pathfilename , p->pathfilename ) ;
		if( result < 0 )
			node = node->rb_left ;
		else if( result > 0 )
			node = node->rb_right ;
		else
			return p ;
	}
	
	return NULL;
}

void RemoveHtmlCachePathfilenameTreeNode( struct HetaoServer *p_server , struct HtmlCacheSession *p_htmlcache_session )
{
	rb_erase( & (p_htmlcache_session->htmlcache_pathfilename_rbnode) , & (p_server->htmlcache_pathfilename_rbtree) );
	return;
}

