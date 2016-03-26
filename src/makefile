# 此文件由makeobj.sh自动生成
############################################################
# 项目名 : 
# 模块名 : 
# 备  注 : 
############################################################

###### 源文件配置区
#@ c_FILE
c_FILE		=	\
			IDL_hetao_conf.dsc.c \
			LOGC.c \
			rbtree.c \
			list.c \
			fasterjson.c \
			fasterhttp.c \
			main.c \
			MonitorProcess.c \
			WorkerProcess.c \
			WorkerThread.c \
			TimerThread.c \
			OnAcceptingSocket.c \
			OnReceivingSocket.c \
			OnSendingSocket.c \
			Envirment.c \
			Config.c \
			VirtualHostHash.c \
			HtmlCacheSession.c \
			HtmlCacheEventHander.c \
			HtmlCacheWdTree.c \
			HtmlCachePathfilenameTree.c \
			HttpSessionTimeoutTree.c \
			HttpSession.c \
			LeastConnectionCountTree.c \
			Util.c \
			MimeTypeHash.c \
			ProcessHttpRequest.c \
			OnConnectingForward.c \
			OnSendingForward.c \
			OnReceivingForward.c \

###### 目标文件、安装目录配置区
include makeinstall
BIN		=	hetao
BININST		=	$(_BININST)

###### 编译选项
CFLAGS		=	$(_CFLAGS) \

###### 链接选项
LFLAGS		=	$(_LFLAGS) \
			-L$(HOME)/lib \
			-lpthread \
			-lssl \
			-lz \

###### 额外宏定义区
CLEAN_ADDITION	=

###### 加载mktpl模板库
#@ make_all
#@ make_clean
#@ make_install
#@ make_install_BININST
#@ make_install_LIBINST
#@ make_install_HDERINST
#@ make_install_DFTHDERINST
#@ make_uninstall
#@ make_uninstall_BININST
#@ make_uninstall_LIBINST
#@ make_uninstall_HDERINST
#@ make_uninstall_DFTHDERINST
include $(MKTPLDIR)/makeobj_$(MKTPLOS).inc

###### 目标文件依赖关系
hetao		:	$(c_FILE_o) $(c_FILE2_o)
	$(CC) -o $@ $(c_FILE_o) $(c_FILE2_o) $(LFLAGS)

