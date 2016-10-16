/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef HETAO_IN_H
#define HETAO_IN_H

#if ( defined _WIN32 )
#define _CRTDBG_MAP_ALLOC
#include "stdlib.h"
#include "crtdbg.h"
#endif

#include "LOGC.h"
#include "fasterhttp.h"

#include "rbtree.h"
#include "list.h"

#include "pcre.h"

#include "IDL_hetao_conf.dsc.h"

char *strndup (const char *s, size_t n);

#define HETAO_LISTEN_SOCKFDS			"HETAO_LISTEN_SOCKFDS"	/* 环境变量名，用于优雅重启时传给下一辈侦听信息 */
#define HETAO_LOG_PATHFILENAME			"HETAO_LOG_PATHFILENAME"	/* 环境变量名，用于优雅重启时传给下一辈日志文件名 */

#define MAX_EPOLL_EVENTS			10000	/* 每次从epoll取回事件数量 */

#define INIT_HTTP_SESSION_COUNT			100	/* 初始化HTTP通讯会话数量 */
#define INCRE_HTTP_SESSION_COUNT		100	/* 每次补充的HTTP通讯会话数量 */
#define MAX_HTTP_SESSION_COUNT_DEFAULT		100000	/* 缺省的最大HTTP通讯会话数量 */

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
#define DATASESSION_TYPE_PIPE		'P'
#define DATASESSION_TYPE_LISTEN		'L'
#define DATASESSION_TYPE_HTTP		'H'
#define DATASESSION_TYPE_HTMLCACHE	'F'

struct DataSession
{
#if ( defined _WIN32 )
	OVERLAPPED		overlapped ;
#endif
	char			type ;
} ;

/* 重写地址结构 */
#define PATTERN_OVECCOUNT	30
#define TEMPLATE_OVECCOUNT	6

#define TEMPLATE_PATTERN	"(\\([0-9]+\\))"

struct RewriteUrl
{
	char			pattern[ sizeof( ((hetao_conf*)0)->listen[0].website[0].rewrite[0].pattern ) ] ;
	char			new_url[ sizeof( ((hetao_conf*)0)->listen[0].website[0].rewrite[0].new_url ) ] ;
	int			new_url_len ;
	
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
#if ( defined _WIN32 )
	OVERLAPPED		overlapped ;
#endif
	char			type ;
	
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

#if ( defined _WIN32 )
	HANDLE			directory_changes_handler ;
	char			directory_changes_buffer[ MAX_PATH * 2 + 1 ] ;
	/*
	char			directory_changes_buffer_mulitbyte[ MAX_PATH + 1 ] ;
	*/
#endif
} ;

/* 侦听会话结构 */
struct ListenSession
{
#if ( defined _WIN32 )
	OVERLAPPED		overlapped ;
#endif
	char			type ;
	
	struct NetAddress	netaddr ;
#if ( defined _WIN32 )
	LPFN_ACCEPTEX		lpfnAcceptEx ;
	SOCKET			accept_socket ;
	char			acceptex_buf[ (sizeof(struct sockaddr_in)+16) * 2 ] ;
#endif
	
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
#define HTTPSESSION_FLAGS_RECEIVING	0x0001
#define HTTPSESSION_FLAGS_SENDING	0x0002

#define HTTPSESSION_FLAGS_CONNECTING	0x0001
#define HTTPSESSION_FLAGS_CONNECTED	0x0002

struct HttpSession
{
#if ( defined _WIN32 )
	OVERLAPPED		overlapped ;
#endif
	char			type ;
	
	struct ListenSession	*p_listen_session ;
	
	int			flag ;
	struct NetAddress	netaddr ;
	struct VirtualHost	*p_virtualhost ;
	struct HttpUri		http_uri ;
	struct HttpEnv		*http ;
	struct HttpBuffer	*http_buf ;
	SSL			*ssl ;
	int			ssl_accepted ;
#if ( defined _WIN32 )
	BIO			*in_bio ;
	BIO			*out_bio ;
	char			in_bio_buffer[ 4096 + 1 ] ;
	char			out_bio_buffer[ 4096 + 1 ] ;
	int			out_bio_len ;
#endif

	int			forward_flags ;
	struct ForwardServer	*p_forward_server ;
	struct NetAddress	forward_netaddr ;
	struct HttpEnv		*forward_http ;
	SSL			*forward_ssl ;
	int			forward_ssl_connected ;
#if ( defined _WIN32 )
	BIO			*forward_in_bio ;
	BIO			*forward_out_bio ;
	char			forward_in_bio_buffer[ 4096 + 1 ] ;
	char			forward_out_bio_buffer[ 4096 + 1 ] ;
	int			forward_out_bio_len ;
#endif
	
	int			timeout_timestamp ;
	struct rb_node		timeout_rbnode ;
	int			elapse_timestamp ;
	struct rb_node		elapse_rbnode ;
	
	struct list_head	list ;
} ;

struct HttpSessionArray
{
	struct HttpSession	*http_session_array ;
	
	struct list_head	list ;
} ;

/* 网页缓存会话结构 */
struct HtmlCacheSession
{
#if ( defined _WIN32 )
	OVERLAPPED		overlapped ;
#endif
	char			type ;
	
	char			*pathfilename ;
	int			pathfilename_len ;
	struct rb_node		htmlcache_pathfilename_rbnode ;
	
	struct STAT		st ;
	
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

/* IPLIMITS结构 */
struct IpLimits
{
	unsigned int		ip ;
	int			count ;
	
	struct hlist_node	iplimits_node ;
} ;

/* 工作进程共享信息结构 */
struct ProcessInfo
{
	int			pipe[2] ;
	
#if ( defined __linux ) || ( defined __unix )
	pid_t			pid ;
#elif ( defined _WIN32 )
	HANDLE			handle ;
	STARTUPINFO		si ;
	PROCESS_INFORMATION	pi ;
#endif
	
	int			epoll_fd ;
	int			epoll_nfds ;
} ;

/* 主环境结构 */
struct HetaoEnv
{
	char			**argv ;
	char			config_pathfilename[ 256 + 1 ] ;
	
	int			worker_processes ;
	int			cpu_affinity ;
	int			accept_mutex ;
	char			error_log[ sizeof( ((hetao_conf*)0)->error_log ) ] ;
	int			log_level ;
	int			limits__max_http_session_count ;
	int			limits__max_file_cache ;
	int			limits__max_connections_per_ip ;
	int			tcp_options__nodelay ;
	int			tcp_options__nolinger ;
	int			http_options__timeout ;
	int			http_options__elapse ;
	int			http_options__compress_on ;
	int			http_options__forward_disable ;
	
	struct passwd		*pwd ;
	
	char			init_ssl_env_flag ;
	
#if ( defined __linux ) || ( defined __unix )
	int			process_info_shmid ;
#elif ( defined _WIN32 )
	HANDLE			process_info_shmid ;
#endif
	struct ProcessInfo	*process_info_array ;
	struct ProcessInfo	*p_this_process_info ;
	int			process_info_index ;
	
#if ( defined _WIN32 )
	HANDLE			iocp ;
	LPFN_CONNECTEX		lpfnConnectEx ;
#endif
	
	int			mimetype_hashsize ;
	struct hlist_head	*mimetype_hash ;
	
	struct DataSession	pipe_session ;
	
	struct ListenSession	listen_session_list ;
	int			listen_session_count ;
	
	pcre			*new_url_re ;
	
#if ( defined __linux ) || ( defined __unix )
	int			htmlcache_inotify_fd ;
#endif
	struct HtmlCacheSession	htmlcache_session ;
	struct HtmlCacheSession	htmlcache_session_list ;
	int			htmlcache_session_count ;
	struct rb_root		htmlcache_wd_rbtree ;
	struct rb_root		htmlcache_pathfilename_rbtree ;
	
	struct HttpSessionArray	http_session_array_list ;
	int			http_session_used_count ;
	struct HttpSession	http_session_unused_list ;
	int			http_session_unused_count ;
	struct rb_root		http_session_timeout_rbtree_used ;
	struct rb_root		http_session_elapse_rbtree_used ;
	
	struct hlist_head	*iplimits_hash ;
} ;

extern struct HetaoEnv		*g_p_env ;
extern signed char		g_second_elapse ;

extern char			*__HETAO_VERSION ;

int InitVirtualHostHash( struct ListenSession *p_listen_session , int count );
void CleanVirtualHostHash( struct ListenSession *p_listen_session );
int PushVirtualHostHashNode( struct ListenSession *p_listen_session , struct VirtualHost *p_virtualhost );
struct VirtualHost *QueryVirtualHostHashNode( struct ListenSession *p_listen_session , char *domain , int domain_len );

int InitIpLimitsHash( struct HetaoEnv *p_env );
void CleanIpLimitsHash( struct HetaoEnv *p_env );
int IncreaseIpLimitsHashNode( struct HetaoEnv *p_env , unsigned int ip );
int DecreaseIpLimitsHashNode( struct HetaoEnv *p_env , unsigned int ip );

int IncreaseHttpSessions( struct HetaoEnv *p_env , int http_session_incre_count );
struct HttpSession *FetchHttpSessionUnused( struct HetaoEnv *p_env , unsigned int ip );
void SetHttpSessionUnused( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
void SetHttpSessionUnused_05( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
void SetHttpSessionUnused_02( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int ReallocHttpSessionChanged( struct HetaoEnv *p_env , struct HtmlCacheSession *p_htmlcache_session );

int AddHttpSessionTimeoutTreeNode( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
void RemoveHttpSessionTimeoutTreeNode( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int UpdateHttpSessionTimeoutTreeNode( struct HetaoEnv *p_env , struct HttpSession *p_http_session , int timeout_timestamp );
struct HttpSession *GetExpireHttpSessionTimeoutTreeNode( struct HetaoEnv *p_env );

int AddHttpSessionElapseTreeNode( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
void RemoveHttpSessionElapseTreeNode( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int UpdateHttpSessionElapseTreeNode( struct HetaoEnv *p_env , struct HttpSession *p_http_session , int elapse_timestamp );
struct HttpSession *GetExpireHttpSessionElapseTreeNode( struct HetaoEnv *p_env );

int AddHtmlCacheWdTreeNode( struct HetaoEnv *p_env , struct HtmlCacheSession *p_htmlcache_session );
struct HtmlCacheSession *QueryHtmlCacheWdTreeNode( struct HetaoEnv *p_env , int wd );
void RemoveHtmlCacheWdTreeNode( struct HetaoEnv *p_env , struct HtmlCacheSession *p_htmlcache_session );

int AddHtmlCachePathfilenameTreeNode( struct HetaoEnv *p_env , struct HtmlCacheSession *p_htmlcache_session );
struct HtmlCacheSession *QueryHtmlCachePathfilenameTreeNode( struct HetaoEnv *p_env , char *pathfilename );
void RemoveHtmlCachePathfilenameTreeNode( struct HetaoEnv *p_env , struct HtmlCacheSession *p_htmlcache_session );

int RegexReplaceString( pcre *pattern_re , char *url , int url_len , pcre *new_url_re , char *new_url , int *p_new_url_len , int new_url_size );

int InitMimeTypeHash( struct HetaoEnv *p_env , hetao_conf *p_conf );
void CleanMimeTypeHash( struct HetaoEnv *p_env );
int PushMimeTypeHashNode( struct HetaoEnv *p_env , struct MimeType *p_mimetype );
struct MimeType *QueryMimeTypeHashNode( struct HetaoEnv *p_env , char *type , int type_len );

int AddLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server );
void RemoveLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server );
int UpdateLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server );
struct ForwardServer *TravelMinLeastConnectionCountTreeNode( struct VirtualHost *p_virtualhost , struct ForwardServer *p_forward_server );

void FreeHtmlCacheSession( struct HtmlCacheSession *p_htmlcache_session , int free_flag );

int DirectoryWatcherEventHander( struct HetaoEnv *p_env , struct VirtualHost *p_virtualhost );
int HtmlCacheEventHander( struct HetaoEnv *p_env );

void SetDefaultConfig( hetao_conf *p_conf );
void AppendDefaultConfig( hetao_conf *p_conf );
int LoadConfig( char *config_pathfilename , hetao_conf *p_conf , struct HetaoEnv *p_env );
int ConvertConfig( hetao_conf *p_config , struct HetaoEnv *p_env );

int InitEnvirment( struct HetaoEnv *p_env , hetao_conf *p_conf );
void CleanEnvirment( struct HetaoEnv *p_env );
int SaveListenSockets( struct HetaoEnv *p_env );
int LoadOldListenSockets( struct NetAddress **pp_old_netaddr_array , int *p_old_netaddr_array_count );
struct NetAddress *GetListener( struct NetAddress *old_netaddr_array , int old_netaddr_array_count , char *ip , int port );
int CloseUnusedOldListeners( struct NetAddress *p_old_netaddr_array , int old_netaddr_array );

int InitListenEnvirment( struct HetaoEnv *p_env , hetao_conf *p_conf , struct NetAddress *old_netaddr_array , int old_netaddr_array_count );

int MonitorProcess( void *pv );

int WorkerProcess( void *pv );
void *WorkerThread( void *pv );

#if ( defined __linux ) || ( defined __unix )
void *TimerThread( void *pv );
#elif ( defined _WIN32 )
DWORD WINAPI TimerThread( LPVOID lpParameter );
#endif

int BindDaemonServer( int (* ServerMain)( void *pv ) , void *pv );
int IsDirectory( char *pathdirname );
int IsFile( char *pathdirname );
int AccessFileExist( char *pathfilename );
int BindCpuAffinity( int processor_no );
unsigned long CalcHash( char *str , int len );

int OnAcceptingSocket( struct HetaoEnv *p_env , struct ListenSession *p_listen_session );
int OnAcceptingSslSocket( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int OnReceivingSocket( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int OnSendingSocket( struct HetaoEnv *p_env , struct HttpSession *p_http_session );

int ProcessHttpRequest( struct HetaoEnv *p_env , struct HttpSession *p_http_session , char *pathname , char *filename , int filename_len );

int SelectForwardAddress( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int ConnectForwardServer( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int OnConnectingForward( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int OnConnectingSslForward( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int OnSendingForward( struct HetaoEnv *p_env , struct HttpSession *p_http_session );
int OnReceivingForward( struct HetaoEnv *p_env , struct HttpSession *p_http_session );

#if ( defined _WIN32 )

int InstallService( char *pszConfigPathfilename );
int UninstallService();
int RunService();
void WINAPI ServiceCtrlHandler( DWORD dwControl );
void WINAPI ServiceMainProc( DWORD argc , LPTSTR *argv );

#endif

#endif
