/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int RegexReplaceString( pcre *pattern_re , char *url , int url_len , pcre *template_re , char *new_url , int *p_new_url_len , int new_url_size )
{
	int		pattern_ovector[ PATTERN_OVECCOUNT ] ;
	int		pattern_ovector_count ;
	
	memset( pattern_ovector , 0x00 , sizeof(pattern_ovector) );
	DebugLog( __FILE__ , __LINE__ , "pcre_exec[%d][%.*s] ..." , url_len , url_len , url );
	pattern_ovector_count = pcre_exec( pattern_re , NULL , url , url_len , 0 , 0 , pattern_ovector , PATTERN_OVECCOUNT ) ;
	DebugLog( __FILE__ , __LINE__ , "pcre_exec[%d][%.*s] return[%d] pattern_ovector[%d][%d][%d][%d][%d][%d][...]" , url_len , url_len , url , pattern_ovector_count , pattern_ovector[2] , pattern_ovector[3] , pattern_ovector[4] , pattern_ovector[5] , pattern_ovector[6] , pattern_ovector[7] );
	if( pattern_ovector_count > 0 )
	{
		char		*p_new_url = NULL ;
		int		remain_len ;
		int		new_url_len ;
		
		int		template_ovector[ PATTERN_OVECCOUNT ] ;
		int		template_ovector_count ;
		
		p_new_url = new_url ;
		remain_len = (*p_new_url_len) ;
		new_url_len = (*p_new_url_len) ;
		while(1)
		{
			memset( template_ovector , 0x00 , sizeof(template_ovector) );
			DebugLog( __FILE__ , __LINE__ , "pcre_exec[%d][%.*s][%d][%.*s] ..." , new_url_len , new_url_len , new_url , remain_len , remain_len , p_new_url );
			template_ovector_count = pcre_exec( template_re , NULL , p_new_url , remain_len , 0 , 0 , template_ovector , TEMPLATE_OVECCOUNT ) ;
			DebugLog( __FILE__ , __LINE__ , "pcre_exec[%d][%.*s][%d][%.*s] return[%d] template_ovector[%d][%d]" , new_url_len , new_url_len , new_url , remain_len , remain_len , p_new_url , template_ovector_count , template_ovector[2] , template_ovector[3] );
			if( template_ovector_count > 0 )
			{
				int	index ;
				char	*p_index = NULL ;
				char	*p_index_end = NULL ;
				
				int	dd_template_ovector_len ;
				int	dd_pattern_ovector_len ;
				int	dd_len ;
				
				for( index = 0 , p_index = p_new_url+template_ovector[2]+1 , p_index_end = p_new_url+template_ovector[3]-1 ; p_index < p_index_end ; p_index++ )
				{
					index = index * 10 + (*p_index)-'0' ;
				}
				if( index > pattern_ovector_count-1 )
				{
					return -11;
				}
				
				dd_template_ovector_len = template_ovector[3] - template_ovector[2] ;
				dd_pattern_ovector_len = pattern_ovector[index*2+1] - pattern_ovector[index*2] ;
				dd_len = dd_pattern_ovector_len - dd_template_ovector_len ;
				new_url_len += dd_len ;
				if( new_url_len > new_url_size-1 )
				{
					return -12;
				}
				
				if( dd_len )
				{
					memmove( p_new_url+template_ovector[3]+dd_len , p_new_url+template_ovector[3] , new_url_len-template_ovector[3] );
				}
				memcpy( p_new_url+template_ovector[2] , url+pattern_ovector[index*2] , pattern_ovector[index*2+1]-pattern_ovector[index*2] );
				
				p_new_url = p_new_url + template_ovector[2] + dd_pattern_ovector_len + 1 ;
				remain_len = new_url_len - dd_pattern_ovector_len ;
			}
			else if( template_ovector_count == 0 || template_ovector_count == PCRE_ERROR_NOMATCH )
			{
				break;
			}
			else
			{
				return 1;
			}
		}
		
		(*p_new_url_len) = new_url_len ;
		return 0;
	}
	else if( pattern_ovector_count == 0 || pattern_ovector_count == PCRE_ERROR_NOMATCH )
	{
		return 1;
	}
	else
	{
		return -1;
	}
}

