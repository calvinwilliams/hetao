# 此文件由makeobj.sh自动生成
############################################################
# 项目名 : 
# 模块名 : 
# 备  注 : 
############################################################

###### 源文件配置区
#@ c_FILE
c_FILE		=	\
			ListenSession.c \
			LOGC.c \
			rbtree.c \
			list.c \
			fasterjson.c \
			fasterhttp.c \
			IDL_hetao_conf.dsc.c \
			Util.c \
			Config.c \
			Envirment.c \
			hetao.c \
			MonitorProcess.c \
			WorkerProcess.c \
			WorkerThread.c \
			TimerThread.c \
			OnAcceptingSocket.c \
			OnReceivingSocket.c \
			OnSendingSocket.c \
			VirtualHostHash.c \
			ProcessHttpRequest.c \
			OnConnectingForward.c \
			OnSendingForward.c \
			OnReceivingForward.c \
			HttpSession.c \
			HtmlCacheSession.c \
			HtmlCacheEventHander.c \
			HtmlCacheWdTree.c \
			HtmlCachePathfilenameTree.c \
			HttpSessionTimeoutTree.c \
			HttpSessionElapseTree.c \
			LeastConnectionCountTree.c \
			MimeTypeHash.c \
			RewriteUrl.c \

c_FILE2		=	\
			LOGC.c \
			fasterjson.c \
			fasterhttp.c \
			IDL_hetao_conf.dsc.c \
			Util.c \
			Config.c \
			hetaocheck.c \

###### 目标文件、安装目录配置区
NOCLEAN_DIRINST_NOCOVER=	/var/hetao
NOCLEAN_DIRINST2_NOCOVER=	/var/hetao/log
BIN			=	hetaocheck hetao
BININST			=	/usr/local/bin
NOCLEAN_OBJ		=	../bin/hetao.sh
NOCLEAN_OBJINST		=	/usr/local/bin
NOCLEAN_OBJ_NOCOVER	=	../conf/hetao.conf
NOCLEAN_OBJINST_NOCOVER	=	/etc/hetao
NOCLEAN_OBJ2_NOCOVER	=	../certs/*
NOCLEAN_OBJINST2_NOCOVER=	/etc/hetao/certs
NOCLEAN_OBJ3_NOCOVER	=	../www/*
NOCLEAN_OBJINST3_NOCOVER=	/var/hetao/www

###### 编译选项
CFLAGS		=	-I.

###### 链接选项
LFLAGS		=	-L. \
			-lpcre \
			-lpthread \
			-lssl \
			-lz \

###### 额外宏定义区
CLEAN_ADDITION	=

###### 加载mktpl模板库
#@ make_all
#@ make_clean
#@ make_install
#@ make_install_NOCLEAN_DIRINST_NOCOVER
#@ make_install_NOCLEAN_DIRINST2_NOCOVER
#@ make_install_BININST
#@ make_install_NOCLEAN_OBJINST
#@ make_install_NOCLEAN_OBJINST_NOCOVER
#@ make_install_NOCLEAN_OBJINST2_NOCOVER
#@ make_install_NOCLEAN_OBJINST3_NOCOVER
#@ make_uninstall
#@ make_uninstall_BININST
#@ make_uninstall_NOCLEAN_OBJINST
include $(MKTPLDIR)/makeobj_$(MKTPLOS).inc

###### 目标文件依赖关系
hetaocheck	:	$(c_FILE2_o)
	$(CC) -o $@ $(c_FILE2_o) $(LFLAGS)

hetao		:	$(c_FILE_o)
	$(CC) -o $@ $(c_FILE_o) $(LFLAGS)
	@echo "	=== INSTALL CONFIG ========= OTHER OBJECT DIRECTORIES BY EDITING makefile$(MAKEFILE_POSTFIX)"
	@echo "	= $(MKDIR) $(NOCLEAN_DIRINST_NOCOVER)"
	@echo "	= $(MKDIR) $(NOCLEAN_DIRINST2_NOCOVER)"
	@echo "	= $(BIN) -> $(BININST)"
	@echo "	= $(NOCLEAN_OBJ) -> $(NOCLEAN_OBJINST)"
	@echo "	= $(NOCLEAN_OBJ_NOCOVER) -> $(NOCLEAN_OBJINST_NOCOVER)"
	@echo "	= $(NOCLEAN_OBJ2_NOCOVER) -> $(NOCLEAN_OBJINST2_NOCOVER)"
	@echo "	= $(NOCLEAN_OBJ3_NOCOVER) -> $(NOCLEAN_OBJINST3_NOCOVER)"
	@echo "	=== EXECUTE FOLLOWING COMMAND TO INSTALL ========="
	@echo "	sudo make -f makefile.$(MKTPLOS) install"

