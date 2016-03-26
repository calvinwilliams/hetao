/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_HETAO_IN_
#define _H_HETAO_IN_

#if ( defined __linux ) || ( defined __unix )
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#ifndef EPOLLRDHUP
#define EPOLLRDHUP	0x2000
#endif
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <dirent.h>
#include "pcre.h"
#define __USE_GNU
#include <sched.h>
#include <pthread.h>
#include <dlfcn.h>
#define _VSNPRINTF			vsnprintf
#define _SNPRINTF			snprintf
#define _CLOSESOCKET			close
#define _CLOSESOCKET2(_fd1_,_fd2_)	close(_fd1_),close(_fd2_);
#define _ERRNO				errno
#define _EWOULDBLOCK			EWOULDBLOCK
#define _ECONNABORTED			ECONNABORTED
#define _EINPROGRESS			EINPROGRESS
#define _ECONNRESET			ECONNRESET
#define _ENOTCONN			ENOTCONN
#define _EISCONN			EISCONN
#define _SOCKLEN_T			socklen_t
#define _GETTIMEOFDAY(_tv_)		gettimeofday(&(_tv_),NULL)
#define _LOCALTIME(_tt_,_stime_) \
	localtime_r(&(_tt_),&(_stime_));
#define _ACCESS				access
#define _ACCESS_MODE			R_OK
#elif ( defined _WIN32 )
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <io.h>
#include <windows.h>
#define _VSNPRINTF			_vsnprintf
#define _SNPRINTF			_snprintf
#define _CLOSESOCKET			closesocket
#define _CLOSESOCKET2(_fd1_,_fd2_)	closesocket(_fd1_),closesocket(_fd2_);
#define _ERRNO				GetLastError()
#define _EWOULDBLOCK			WSAEWOULDBLOCK
#define _ECONNABORTED			WSAECONNABORTED
#define _EINPROGRESS			WSAEINPROGRESS
#define _ECONNRESET			WSAECONNRESET
#define _ENOTCONN			WSAENOTCONN
#define _EISCONN			WSAEISCONN
#define _SOCKLEN_T			int
#define _GETTIMEOFDAY(_tv_) \
	{ \
		SYSTEMTIME stNow ; \
		GetLocalTime( & stNow ); \
		(_tv_).tv_usec = stNow.wMilliseconds * 1000 ; \
		time( & ((_tv_).tv_sec) ); \
	}
#define _SYSTEMTIME2TIMEVAL_USEC(_syst_,_tv_) \
		(_tv_).tv_usec = (_syst_).wMilliseconds * 1000 ;
#define _SYSTEMTIME2TM(_syst_,_stime_) \
		(_stime_).tm_year = (_syst_).wYear - 1900 ; \
		(_stime_).tm_mon = (_syst_).wMonth - 1 ; \
		(_stime_).tm_mday = (_syst_).wDay ; \
		(_stime_).tm_hour = (_syst_).wHour ; \
		(_stime_).tm_min = (_syst_).wMinute ; \
		(_stime_).tm_sec = (_syst_).wSecond ;
#define _LOCALTIME(_tt_,_stime_) \
	{ \
		SYSTEMTIME	stNow ; \
		GetLocalTime( & stNow ); \
		_SYSTEMTIME2TM( stNow , (_stime_) ); \
	}
#define _ACCESS				_access
#define _ACCESS_MODE			04
#endif

#include "fasterhttp.h"
#include "LOGC.h"
#include "rbtree.h"
#include "list.h"

#include "IDL_hetao_conf.dsc.h"

#define HETAO_LISTEN_SOCKFDS			"HETAO_LISTEN_SOCKFDS"	/* 环境变量名，用于优雅重启时传给下一辈侦听信息 */
#define HETAO_LOG_PATHFILENAME			"HETAO_LOG_PATHFILENAME"	/* 环境变量名，用于优雅重启时传给下一辈日志文件名 */

#define MAX_EPOLL_EVENTS			10000	/* 每次从epoll取回事件数量 */

#define INIT_HTTP_SESSION_COUNT			100	/* 初始化HTTP通讯会话数量 */
#define INCRE_HTTP_SESSION_COUNT		100	/* 每次补充的HTTP通讯会话数量 */
#define MAX_HTTP_SESSION_COUNT_DEFAULT		100000	/* 缺省的最大HTTP通讯会话数量 */

/* 会话类型 */
#define DATASESSION_TYPE_PIPE			'P'
#define DATASESSION_TYPE_LISTEN			'L'
#define DATASESSION_TYPE_HTMLCACHE		'C'
#define DATASESSION_TYPE_HTTP			'A'

#define SIGNAL_REOPEN_LOG			'L' /* 重新打开日志 */

/* 功能宏 */
#define SETNETADDRESS(_netaddr_) \
	memset( & ((_netaddr_).addr) , 0x00 , sizeof(struct sockaddr_in) ); \
	(_netaddr_).addr.sin_family = AF_INET ; \
	if( (_netaddr_).ip[0] == '\0' ) \
		(_netaddr_).addr.sin_addr.s_addr = INADDR_ANY ; \
	else \
		(_netaddr_).addr.sin_addr.s_addr = inet_addr((_netaddr_).ip) ; \
	(_netaddr_).addr.sin_port = htons( (unsigned short)((_netaddr_).port) );

#define GETNETADDRESS(_netaddr_) \
	strcpy( (_netaddr_).ip , inet_ntoa((_netaddr_).addr.sin_addr) ); \
	(_netaddr_).port = (int)ntohs( (_netaddr_).addr.sin_port ) ;

/* 网络信息结构 */
struct NetAddress
{
	char			ip[ sizeof( ((hetao_conf*)0)->listen[0].website[0].domain ) + 1 ] ;
	int			port ;
	SOCKET			sock ;
	struct sockaddr_in	addr ;
} ;

/* 泛数据会话结构，type为其它会话做预判断 */
struct DataSession
{
	char			type ;
} ;

/* 重写地址结构 */
#define PATTERN_OVECCOUNT	30
#define TEMPLATE_OVECCOUNT	6

#define TEMPLATE_PATTERN	"(\\([0-9]+\\))"

struct RewriteUrl
{
	char			pattern[ sizeof( ((hetao_conf*)0)->listen[0].website[0].rewrite[0].pattern ) ] ;
	char			template[ sizeof( ((hetao_conf*)0)->listen[0].website[0].rewrite[0].template ) ] ;
	int			template_len ;
	
	pcre			*pattern_re ;
	
	struct list_head	rewriteurl_node ;
} ;

/* 转发服务器结构 */
struct ForwardServer
{
	time_t			timestamp_to_valid ;
	
	struct NetAddress	netaddr ;
	
	int			connection_count ;
	
	struct list_head	roundrobin_node ;
	struct rb_node		leastconnection_rbnode ;
} ;

/* 虚拟主机结构 */
#define FORWARD_RULE_ROUNDROBIN			"R"
#define FORWARD_RULE_LEASTCONNECTION		"L"

struct VirtualHost
{
	char			domain[ sizeof( ((hetao_conf*)0)->listen[0].website[0].domain ) ] ;
	char			wwwroot[ sizeof( ((hetao_conf*)0)->listen[0].website[0].wwwroot ) ] ;
	char			index[ sizeof( ((hetao_conf*)0)->listen[0].website[0].index ) ] ;
	char			access_log[ sizeof( ((hetao_conf*)0)->listen[0].website[0].access_log ) ] ;
	
	int			domain_len ;
	int			access_log_fd ;
	
	struct RewriteUrl	rewrite_url_list ;
	
	char			forward_type[ sizeof( ((hetao_conf*)0)->listen[0].website[0].forward.forward_type ) ] ;
	int			forward_type_len ;
	char			forward_rule[ sizeof( ((hetao_conf*)0)->listen[0].website[0].forward.forward_rule ) ] ;
	struct ForwardServer	roundrobin_list ;
	struct rb_root		leastconnection_rbtree ;
	
	SSL_CTX			*forward_ssl_ctx ;
	
	struct hlist_node	virtualhost_node ;
} ;

/* 侦听会话结构 */
struct ListenSession
{
	char			type ;
	
	struct NetAddress	netaddr ;
	
	SSL_CTX			*ssl_ctx ;
	
	struct VirtualHost	*p_virtualhost_default ;
	int			virtualhost_hashsize ;
	int			virtualhost_count ;
	struct hlist_head	*virtualhost_hash ;
	
	struct list_head	list ;
} ;

/* 流类型结构 */
struct MimeType
{
	char			type[ sizeof( ((hetao_conf*)0)->mime_types.mime_type[0].type ) ] ;
	char			mime[ sizeof( ((hetao_conf*)0)->mime_types.mime_type[0].mime ) ] ;
	char			compress_enable ;
	
	int			type_len ;
	
	struct hlist_node	mimetype_node ;
} ;

/* HTTP通讯会话 */
#define HTTPSESSION_FLAGS_CONNECTING	0x0001
#define HTTPSESSION_FLAGS_CONNECTED	0x0002

struct HttpSession
{
	char			type ;
	
	struct ListenSession	*p_listen_session ;
	
	struct NetAddress	netaddr ;
	struct VirtualHost	*p_virtualhost ;
	struct HttpUri		http_uri ;
	struct HttpEnv		*http ;
	SSL			*ssl ;
	
	int			forward_flags ;
	struct ForwardServer	*p_forward_server ;
	SOCKET			forward_sock ;
	struct HttpEnv		*forward_http ;
	SSL			*forward_ssl ;
	
	int			timeout_timestamp ;
	struct rb_node		timeout_rbnode ;
	
	struct list_head	list ;
} ;

/* 网页缓存会话结构 */
struct HtmlCacheSession
{
	char			type ;
	
	char			*pathfilename ;
	int			pathfilename_len ;
	struct rb_node		htmlcache_pathfilename_rbnode ;
	
	struct stat		st ;
	
	char			*html_content ;
	int			html_content_len ;
	char			*html_gzip_content ;
	int			html_gzip_content_len ;
	char			*html_deflate_content ;
	int			html_deflate_content_len ;
	
	int			wd ;
	struct rb_node		htmlcache_wd_rbnode ;
	
	struct list_head	list ;
} ;

/* 工作进程共享信息结构 */
struct ProcessInfo
{
	int			pipe[2] ;
	
	pid_t			pid ;
	
	int			epoll_fd ;
	int			epoll_nfds ;
} ;

/* 主环境结构 */
struct HetaoEnv
{
	char				**argv ;
	char				config_pathfilename[ 256 + 1 ] ;
	hetao_conf			*p_config ;
	int				log_level ;
	
	char				init_ssl_env_flag ;
	
	int				process_info_shmid ;
	struct ProcessInfo		*process_info_array ;
	struct ProcessInfo		*p_this_process_info ;
	int				process_info_index ;
	
	int				mimetype_hashsize ;
	struct hlist_head		*mimetype_hash ;
	
	struct DataSession		pipe_session ;
	
	struct ListenSession		listen_session_list ;
	int				listen_session_count ;
	
	pcre				*template_re ;
	
	int				htmlcache_inotify_fd ;
	struct HtmlCacheSession		htmlcache_session ;
	struct HtmlCacheSession		htmlcache_session_list ;
	int				htmlcache_session_count ;
	struct rb_root			htmlcache_wd_rbtree ;
	struct rb_root			htmlcache_pathfilename_rbtree ;
	
	int				http_session_used_count ;
	struct HttpSession		http_session_unused_list ;
	int				http_session_unused_count ;
	struct rb_root			http_session_rbtree_used ;
} ;

extern struct HetaoEnv		*g_p_env ;
extern signed char		g_second_elapse ;

extern char			*__HETAO_VERSION ;

int InitVirtualHostHash( struct ListenSession *p_listen_session , int count );
void CleanVirtualHostHash( struct ListenSession *p_listen_session );
int PushVirtualHostHashNode( struct ListenSession *p_listen_session , struct VirtualHost *p_virtualhost );
struct VirtualHost *QueryVirtualHostHashNode( struct ListenSession *p_listen_session , char *domain , int domain_len );

int IncreaseHttpSessions( struct HetaoEnv *p_server , int http_session_incre_count );
struct HttpSession *FetchHttpSessionUnused( struct HetaoEnv *p_server );
void SetHttpSessionUnused( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
void SetHttpSessionUnused_05( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
void SetHttpSessionUnused_02( struct HetaoEnv *p_server , struct HttpSession *p_http_session );

int AddHttpSessionTimeoutTreeNode( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
void RemoveHttpSessionTimeoutTreeNode( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
int UpdateHttpSessionTimeoutTreeNode( struct HetaoEnv *p_server , struct HttpSession *p_http_session , int timeout_timestamp );
struct HttpSession *GetExpireHttpSessionTimeoutTreeNode( struct HetaoEnv *p_server );

int AddHtmlCacheWdTreeNode( struct HetaoEnv *p_server , struct HtmlCacheSession *p_htmlcache_session );
struct HtmlCacheSession *QueryHtmlCacheWdTreeNode( struct HetaoEnv *p_server , int wd );
void RemoveHtmlCacheWdTreeNode( struct HetaoEnv *p_server , struct HtmlCacheSession *p_htmlcache_session );

int AddHtmlCachePathfilenameTreeNode( struct HetaoEnv *p_server , struct HtmlCacheSession *p_htmlcache_session );
struct HtmlCacheSession *QueryHtmlCachePathfilenameTreeNode( struct HetaoEnv *p_server , char *pathfilename );
void RemoveHtmlCachePathfilenameTreeNode( struct HetaoEnv *p_server , struct HtmlCacheSession *p_htmlcache_session );

int RegexReplaceString( pcre *pattern_re , char *url , int url_len , pcre *template_re , char *new_url , int *p_new_url_len , int new_url_size );

int InitMimeTypeHash( struct HetaoEnv *p_server );
void CleanMimeTypeHash( struct HetaoEnv *p_server );
int PushMimeTypeHashNode( struct HetaoEnv *p_server , struct MimeType *p_mimetype );
struct MimeType *QueryMimeTypeHashNode( struct HetaoEnv *p_server , char *type , int type_len );

int AddLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server );
void RemoveLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server );
int UpdateLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server );
struct ForwardServer *TravelMinLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server );

void FreeHtmlCacheSession( struct HtmlCacheSession *p_htmlcache_session , int free_flag );

int OnSendingSocket( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
int OnReceivingSocket( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
int OnAcceptingSocket( struct HetaoEnv *p_server , struct ListenSession *p_listen_session );

int DirectoryWatcherEventHander( struct HetaoEnv *p_server );
int HtmlCacheEventHander( struct HetaoEnv *p_server );

int LoadConfig( char *config_pathfilename , struct HetaoEnv *p_server );

int InitEnvirment( struct HetaoEnv *p_server );
void CleanEnvirment( struct HetaoEnv *p_server );
int SaveListenSockets( struct HetaoEnv *p_server );
int LoadOldListenSockets( struct NetAddress **pp_old_netaddr_array , int *p_old_netaddr_array_count );
struct NetAddress *GetListener( struct NetAddress *old_netaddr_array , int old_netaddr_array_count , char *ip , int port );
int CloseUnusedOldListeners( struct NetAddress *p_old_netaddr_array , int old_netaddr_array );

int InitListenEnvirment( struct HetaoEnv *p_env , struct NetAddress *old_netaddr_array , int old_netaddr_array_count );

int MonitorProcess( void *pv );

int WorkerProcess( void *pv );
void *WorkerThread( void *pv );
void *TimerThread( void *pv );

int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv );
int AccessDirectoryExist( char *pathdirname );
int AccessFileExist( char *pathfilename );
int BindCpuAffinity( int processor_no );
unsigned long CalcHash( char *str , int len );

int ProcessHttpRequest( struct HetaoEnv *p_server , struct HttpSession *p_http_session , char *pathname , char *filename , int filename_len );

int SelectForwardAddress( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
int ConnectForwardServer( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
int OnConnectingForward( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
int OnSendingForward( struct HetaoEnv *p_server , struct HttpSession *p_http_session );
int OnReceivingForward( struct HetaoEnv *p_server , struct HttpSession *p_http_session );

#endif

