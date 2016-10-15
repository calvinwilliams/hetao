
// minihetao.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "minihetao.h"
#include "minihetaoDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMiniHetaoApp

BEGIN_MESSAGE_MAP(CMiniHetaoApp, CWinAppEx)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CMiniHetaoApp 构造

CMiniHetaoApp::CMiniHetaoApp()
{
	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的一个 CMiniHetaoApp 对象

CMiniHetaoApp theApp;


// CMiniHetaoApp 初始化

BOOL CMiniHetaoApp::InitInstance()
{
	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();

	AfxEnableControlContainer();

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

	// 显示对话框
	CMiniHetaoDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: 在此放置处理何时用
		//  “确定”来关闭对话框的代码
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 在此放置处理何时用
		//  “取消”来关闭对话框的代码
	}

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

extern "C"
{
#include "../../hetao_in.h"
}

#define PREFIX_DSCLOG_hetao_conf	DebugLog( __FILE__ , __LINE__ , 
#define NEWLINE_DSCLOG_hetao_conf
#include "../../IDL_hetao_conf.dsc.LOG.c"

#if ( defined _WIN32 )
static	WSADATA		wsd;
#endif

DWORD WINAPI minihetao( LPVOID p )
{
	char			*wwwroot = (char*)p ;
	hetao_conf		*p_conf = NULL ;
	struct HetaoEnv		*p_env = NULL ;
	
	int			nret = 0 ;
	
	/* 设置标准输出无缓冲 */
	setbuf( stdout , NULL );
	
	/* 设置随机数种子 */
	srand( (unsigned)time(NULL) );
	
	/* 设置文件掩码0 */
	UMASK(0);
	
	if( p )
	{
		if( WSAStartup( MAKEWORD(2,2) , & wsd ) != 0 )
			return 1;
		/* 申请环境结构内存 */
		p_env = (struct HetaoEnv *)malloc( sizeof(struct HetaoEnv) ) ;
		if( p_env == NULL )
		{
			return 1;
		}
		memset( p_env , 0x00 , sizeof(struct HetaoEnv) );
		g_p_env = p_env ;
		p_env->argv = NULL ;
		
		/* 申请配置结构内存 */
		p_conf = (hetao_conf *)malloc( sizeof(hetao_conf) ) ;
		if( p_conf == NULL )
		{
			return 1;
		}
		memset( p_conf , 0x00 , sizeof(hetao_conf) );
		
		/* 重置所有HTTP状态码、描述为缺省 */
		ResetAllHttpStatus();
		
		/* 设置缺省主日志 */
		if( getenv( HETAO_LOG_PATHFILENAME ) == NULL )
			SetLogFile( "#" );
		else
			SetLogFile( getenv(HETAO_LOG_PATHFILENAME) );
		SetLogLevel( LOGLEVEL_ERROR );
		SETPID
		SETTID
		UPDATEDATETIMECACHEFIRST
		
		/* 设置缺省配置 */
		SetDefaultConfig( p_conf );
		
		if( IsDirectory( wwwroot ) != 1 )
		{
			return 1;
		}
		
		strcpy( p_conf->listen[0].ip , "" );
		p_conf->listen[0].port = 80 ;
		p_conf->_listen_count = 1 ;
		
		strcpy( p_conf->listen[0].website[0].domain , "" );
		strcpy( p_conf->listen[0].website[0].wwwroot , wwwroot );
		strcpy( p_conf->listen[0].website[0].index , "/index.html,/index.htm" );
		strcpy( p_conf->listen[0].website[0].access_log , "" );
		p_conf->listen[0]._website_count = 1 ;
		
		/* 追加缺省配置 */
		AppendDefaultConfig( p_conf );
		
		/* 转换配置 */
		nret = ConvertConfig( p_conf , p_env ) ;
		if( nret )
		{
			free( p_conf );
			free( p_env );
			return -nret;
		}
		
		/* 重新设置主日志 */
		CloseLogFile();
		
		SetLogFile( p_env->error_log );
		SetLogLevel( p_env->log_level );
		SETPID
		SETTID
		UPDATEDATETIMECACHEFIRST
		InfoLog( __FILE__ , __LINE__ , "--- hetao v%s build %s %s ---" , __HETAO_VERSION , __DATE__ , __TIME__ );
		SetHttpCloseExec( g_file_fd );
		
		/* 输出配置到日志 */
		DSCLOG_hetao_conf( p_conf );
		
		/* 初始化服务器环境 */
		nret = InitEnvirment( p_env , p_conf ) ;
		free( p_conf );
		if( nret )
		{
			return -nret;
		}
		
		return -WorkerProcess( p_env );
	}
	
	return 0;
}
