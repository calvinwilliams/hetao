/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int AddLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server )
{
        struct rb_node		**pp_new_node = NULL ;
        struct rb_node		*p_parent = NULL ;
        struct ForwardServer	*p = NULL ;
	
	pp_new_node = & (p_virtualhost->leastconnection_rbtree.rb_node) ;
        while( *pp_new_node )
        {
                p = container_of( *pp_new_node , struct ForwardServer , leastconnection_rbnode ) ;
		
                p_parent = (*pp_new_node) ;
		
                if( p_forward_server->connection_count < p->connection_count )
                        pp_new_node = &((*pp_new_node)->rb_left) ;
                else if( p_forward_server->connection_count > p->connection_count )
                        pp_new_node = &((*pp_new_node)->rb_right) ;
                else 
                        pp_new_node = &((*pp_new_node)->rb_left) ;
        }
	
        rb_link_node( & (p_forward_server->leastconnection_rbnode) , p_parent , pp_new_node );
        rb_insert_color( & (p_forward_server->leastconnection_rbnode) , & (p_virtualhost->leastconnection_rbtree) );
	
	return 0;
}

void RemoveLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server )
{
	rb_erase( & (p_forward_server->leastconnection_rbnode) , & (p_virtualhost->leastconnection_rbtree) );
	return;
}

int UpdateLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server )
{
	int		nret = 0 ;
	
	RemoveLeastConnectionCountTreeNode( p_virtualhost , p_forward_server );
	
	nret = AddLeastConnectionCountTreeNode( p_virtualhost , p_forward_server ) ;
	
	return nret;
}

struct ForwardServer *TravelMinLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server )
{
	struct rb_node		*p_curr = NULL ;
	
	if( p_forward_server == NULL )
	{
		p_curr = rb_first( & (p_virtualhost->leastconnection_rbtree) ); 
		if (p_curr == NULL)
			return NULL;
	}
	else
	{
		p_curr = rb_next( & (p_forward_server->leastconnection_rbnode) ) ;
		if (p_curr == NULL)
			return NULL;
	}
	
	return rb_entry( p_curr , struct ForwardServer , leastconnection_rbnode );
}

