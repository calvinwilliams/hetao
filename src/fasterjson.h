#ifndef _H_FASTERJSON_
#define _H_FASTERJSON_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* util */

#if ( defined _WIN32 )
#ifndef _WINDLL_FUNC
#define _WINDLL_FUNC		_declspec(dllexport)
#endif
#elif ( defined __unix ) || ( defined __linux__ )
#ifndef _WINDLL_FUNC
#define _WINDLL_FUNC		extern
#endif
#endif

#define JSONENCODING_SKIP_MULTIBYTE								\
				if( g_fasterjson_encoding == FASTERJSON_ENCODING_UTF8 )		\
				{								\
					if( (*(_p_src_))>>5 == 0x06 && (*(_p_src_+1))>>6 == 0x02 )	\
					{							\
						_remain_len_-=2; if( _remain_len_ < 0 ) break;	\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
					}							\
					else if( (*(_p_src_))>>4 == 0x0E && (*(_p_src_+1))>>6 == 0x02 && (*(_p_src_+2))>>6 == 0x02 )	\
					{							\
						_remain_len_-=3; if( _remain_len_ < 0 ) break;	\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
					}							\
					else if( (*(_p_src_))>>3 == 0x1E && (*(_p_src_+1))>>6 == 0x02 && (*(_p_src_+2))>>6 == 0x02 && (*(_p_src_+3))>>6 == 0x02 )	\
					{							\
						_remain_len_-=4; if( _remain_len_ < 0 ) break;	\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
					}							\
					else if( (*(_p_src_))>>2 == 0x3E && (*(_p_src_+1))>>6 == 0x02 && (*(_p_src_+2))>>6 == 0x02 && (*(_p_src_+3))>>6 == 0x02 && (*(_p_src_+4))>>6 == 0x02 )	\
					{							\
						_remain_len_-=4; if( _remain_len_ < 0 ) break;	\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
					}							\
					else if( (*(_p_src_))>>1 == 0x7E && (*(_p_src_+1))>>6 == 0x02 && (*(_p_src_+2))>>6 == 0x02 && (*(_p_src_+3))>>6 == 0x02 && (*(_p_src_+4))>>6 == 0x02 && (*(_p_src_+5))>>6 == 0x02 )	\
					{							\
						_remain_len_-=4; if( _remain_len_ < 0 ) break;	\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
						*(_p_dst_++) = *(_p_src_++) ;			\
					}							\
					else							\
					{							\
						_remain_len_--; if( _remain_len_ < 0 ) break;	\
						*(_p_dst_++) = *(_p_src_++) ;			\
					}							\
				}								\
				else if( g_fasterjson_encoding == FASTERJSON_ENCODING_GB18030 )	\
				{								\
					_remain_len_-=2; if( _remain_len_ < 0 ) break;		\
					*(_p_dst_++) = *(_p_src_++) ;				\
					*(_p_dst_++) = *(_p_src_++) ;				\
				}								\
				else								\
				{								\
					_remain_len_--; if( _remain_len_ < 0 ) break;		\
					*(_p_dst_++) = *(_p_src_++) ;				\
				}								\

#define JSONESCAPE_EXPAND(_src_,_src_len_,_dst_,_dst_len_,_dst_remain_len_)			\
	(_dst_len_) = 0 ;									\
	if( (_src_len_) > 0 )									\
	{											\
		unsigned char	*_p_src_ = (unsigned char *)(_src_) ;				\
		unsigned char	*_p_src_end_ = (unsigned char *)(_src_) + (_src_len_) - 1 ;	\
		unsigned char	*_p_dst_ = (unsigned char *)(_dst_) ;				\
		int		_remain_len_ = (_dst_remain_len_) ;				\
		while( _p_src_ <= _p_src_end_ )							\
		{										\
			if( *(_p_src_) > 127 )							\
			{									\
				JSONENCODING_SKIP_MULTIBYTE					\
			}									\
			else if( *(_p_src_) == '\"' || *(_p_src_) == '\\' )			\
			{									\
				_remain_len_-=2; if( _remain_len_ < 0 ) break;			\
				*(_p_dst_++) = '\\' ;						\
				*(_p_dst_++) = *(_p_src_) ;					\
				(_p_src_)++;							\
			}									\
			else if( *(_p_src_) == '\t' )						\
			{									\
				_remain_len_-=2; if( _remain_len_ < 0 ) break;			\
				*(_p_dst_++) = '\\' ;						\
				*(_p_dst_++) = 't' ;						\
				(_p_src_)++;							\
			}									\
			else if( *(_p_src_) == '\r' )						\
			{									\
				_remain_len_-=2; if( _remain_len_ < 0 ) break;			\
				*(_p_dst_++) = '\\' ;						\
				*(_p_dst_++) = 'r' ;						\
				(_p_src_)++;							\
			}									\
			else if( *(_p_src_) == '\n' )						\
			{									\
				_remain_len_-=2; if( _remain_len_ < 0 ) break;			\
				*(_p_dst_++) = '\\' ;						\
				*(_p_dst_++) = 'n' ;						\
				(_p_src_)++;							\
			}									\
			else if( *(_p_src_) == '\b' )						\
			{									\
				_remain_len_-=2; if( _remain_len_ < 0 ) break;			\
				*(_p_dst_++) = '\\' ;						\
				*(_p_dst_++) = 'b' ;						\
				(_p_src_)++;							\
			}									\
			else if( *(_p_src_) == '\f' )						\
			{									\
				_remain_len_-=2; if( _remain_len_ < 0 ) break;			\
				*(_p_dst_++) = '\\' ;						\
				*(_p_dst_++) = 'f' ;						\
				(_p_src_)++;							\
			}									\
			else									\
			{									\
				_remain_len_--; if( _remain_len_ < 0 ) break;			\
				*(_p_dst_++) = *(_p_src_++) ;					\
			}									\
		}										\
		*(_p_dst_) = '\0' ;								\
		if( _remain_len_ < 0 || _p_src_ <= _p_src_end_ )				\
		{										\
			(_dst_len_) = -1 ;							\
		}										\
		else										\
		{										\
			(_dst_len_) = (_p_dst_) - (unsigned char *)(_dst_) ;			\
		}										\
	}											\

#define JSONUNESCAPE_FOLD(_src_,_src_len_,_dst_,_dst_len_,_dst_remain_len_)			\
	(_dst_len_) = 0 ;									\
	if( (_src_len_) > 0 )									\
	{											\
		unsigned char	*_p_src_ = (unsigned char *)(_src_) ;				\
		unsigned char	*_p_src_end_ = (unsigned char *)(_src_) + (_src_len_) - 1 ;	\
		unsigned char	*_p_dst_ = (unsigned char *)(_dst_) ;				\
		int		_remain_len_ = (_dst_remain_len_) ;				\
		while( _p_src_ <= _p_src_end_ )							\
		{										\
			if( *(_p_src_) > 127 )							\
			{									\
				JSONENCODING_SKIP_MULTIBYTE					\
			}									\
			else if( *(_p_src_) == '\\' )						\
			{									\
				if( *(_p_src_+1) == 't' )					\
				{								\
					_remain_len_--; if( _remain_len_ < 0 ) break;		\
					*(_p_dst_++) = '\t' ;					\
					(_p_src_)+=2;						\
				}								\
				else if( *(_p_src_+1) == 'r' )					\
				{								\
					_remain_len_--; if( _remain_len_ < 0 ) break;		\
					*(_p_dst_++) = '\r' ;					\
					(_p_src_)+=2;						\
				}								\
				else if( *(_p_src_+1) == 'n' )					\
				{								\
					_remain_len_--; if( _remain_len_ < 0 ) break;		\
					*(_p_dst_++) = '\n' ;					\
					(_p_src_)+=2;						\
				}								\
				else if( *(_p_src_+1) == 'b' )					\
				{								\
					_remain_len_--; if( _remain_len_ < 0 ) break;		\
					*(_p_dst_++) = '\b' ;					\
					(_p_src_)+=2;						\
				}								\
				else if( *(_p_src_+1) == 'f' )					\
				{								\
					_remain_len_--; if( _remain_len_ < 0 ) break;		\
					*(_p_dst_++) = '\f' ;					\
					(_p_src_)+=2;						\
				}								\
				else if( *(_p_src_+1) == 'u' )					\
				{								\
					if( *(_p_src_+2) == '0' && *(_p_src_+3) == '0' )	\
					{							\
						char	*hexset = "0123456789aAbBcCdDeEfF" ;	\
						char	*p4 , *p5 ;				\
						int	c4 , c5 ;				\
						p4 = strchr( hexset , *(char*)(_p_src_+4) ) ;	\
						p5 = strchr( hexset , *(char*)(_p_src_+5) ) ;	\
						if( p4 == NULL || p5 == NULL )			\
						{						\
							break;					\
						}						\
						c4 = p4 - hexset ;				\
						c5 = p5 - hexset ;				\
						if( c4 >= 10 ) c4 = 10+(c4-10)/2 ;		\
						if( c5 >= 10 ) c5 = 10+(c5-10)/2 ;		\
						_remain_len_--; if( _remain_len_ < 0 ) break;	\
						*(_p_dst_++) = c4 * 16 + c5 ;			\
						(_p_src_)+=6;					\
					}							\
					else							\
					{							\
						char	*hexset = "0123456789aAbBcCdDeEfF" ;	\
						char	*p2 , *p3 , *p4 , *p5 ;			\
						int	c2 , c3 , c4 , c5 ;			\
						p2 = strchr( hexset , *(char*)(_p_src_+2) ) ;	\
						p3 = strchr( hexset , *(char*)(_p_src_+3) ) ;	\
						p4 = strchr( hexset , *(char*)(_p_src_+4) ) ;	\
						p5 = strchr( hexset , *(char*)(_p_src_+5) ) ;	\
						if( p2 == NULL || p3 == NULL || p4 == NULL || p5 == NULL )	\
						{						\
							break;					\
						}						\
						c2 = p2 - hexset ;				\
						c3 = p3 - hexset ;				\
						c4 = p4 - hexset ;				\
						c5 = p5 - hexset ;				\
						if( c2 >= 10 ) c2 = 10+(c2-10)/2 ;		\
						if( c3 >= 10 ) c3 = 10+(c3-10)/2 ;		\
						if( c4 >= 10 ) c4 = 10+(c4-10)/2 ;		\
						if( c5 >= 10 ) c5 = 10+(c5-10)/2 ;		\
						_remain_len_-=2; if( _remain_len_ < 0 ) break;	\
						*(_p_dst_++) = c2 * 16 + c3 ;			\
						*(_p_dst_++) = c4 * 16 + c5 ;			\
						(_p_src_)+=6;					\
					}							\
				}								\
				else								\
				{								\
					_remain_len_--; if( _remain_len_ < 0 ) break;		\
					*(_p_dst_++) = *(_p_src_+1) ;				\
					(_p_src_)+=2;						\
				}								\
			}									\
			else									\
			{									\
				_remain_len_--; if( _remain_len_ < 0 ) break;			\
				*(_p_dst_++) = *(_p_src_++) ;					\
			}									\
		}										\
		*(_p_dst_) = '\0' ;								\
		if( _remain_len_ < 0 || _p_src_ <= _p_src_end_ )				\
		{										\
			(_dst_len_) = -1 ;							\
		}										\
		else										\
		{										\
			(_dst_len_) = (_p_dst_) - (unsigned char *)(_dst_) ;			\
		}										\
	}											\

/* fastjson */

#define FASTERJSON_ERROR_INTERNAL			-110
#define FASTERJSON_ERROR_FILENAME			-111
#define FASTERJSON_ERROR_JSON_INVALID_ON_TOKEN_LEAF_0	-121
#define FASTERJSON_ERROR_JSON_INVALID_ON_TOKEN_LEAF_1	-131
#define FASTERJSON_ERROR_JSON_INVALID_ON_TOKEN_LEAF_2	-132
#define FASTERJSON_ERROR_JSON_INVALID_ON_TOKEN_LEAF_3	-133
#define FASTERJSON_ERROR_JSON_INVALID_ON_TOKEN_LEAF_4	-134
#define FASTERJSON_ERROR_JSON_INVALID_ON_TOKEN_ARRAY_1	-141
#define FASTERJSON_ERROR_JSON_INVALID_ON_TOKEN_ARRAY_2	-142
#define FASTERJSON_ERROR_JSON_INVALID			-150
#define FASTERJSON_ERROR_END_OF_BUFFER			-160

#define FASTERJSON_NODE_BRANCH				0x10
#define FASTERJSON_NODE_LEAF				0x20
#define FASTERJSON_NODE_ARRAY				0x40
#define FASTERJSON_NODE_ENTER				0x01
#define FASTERJSON_NODE_LEAVE				0x02

typedef int funcCallbackOnJsonNode( int type , char *jpath , int jpath_len , int jpath_size , char *node , int node_len , char *content , int content_len , void *p );
_WINDLL_FUNC int TravelJsonBuffer( char *json_buffer , char *jpath , int jpath_size
				, funcCallbackOnJsonNode *pfuncCallbackOnJsonNode
				, void *p );
_WINDLL_FUNC int TravelJsonBuffer4( char *json_buffer , char *jpath , int jpath_size
				, funcCallbackOnJsonNode *pfuncCallbackOnEnterJsonBranch
				, funcCallbackOnJsonNode *pfuncCallbackOnLeaveJsonBranch
				, funcCallbackOnJsonNode *pfuncCallbackOnEnterJsonArray
				, funcCallbackOnJsonNode *pfuncCallbackOnLeaveJsonArray
				, funcCallbackOnJsonNode *pfuncCallbackOnJsonLeaf
				, void *p );

#define FASTERJSON_ENCODING_UTF8		0 /* UTF-8 */
#define FASTERJSON_ENCODING_GB18030		1 /* GB18030/GBK/GB2312 */
_WINDLL_FUNC char		g_fasterjson_encoding ;

#define FASTERJSON_NULL				"\x7F"

#ifdef __cplusplus
}
#endif

#endif

