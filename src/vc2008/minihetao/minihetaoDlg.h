
// minihetaoDlg.h : 头文件
//

#pragma once
#include "afxwin.h"


// CMiniHetaoDlg 对话框
class CMiniHetaoDlg : public CDialog
{
// 构造
public:
	CMiniHetaoDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_MINIHETAO_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();

	afx_msg LRESULT OnClickPopupMenu( WPARAM wParam , LPARAM lParam );
	afx_msg void OnShowWindow();
	afx_msg void OnShowAboutBox();

	DECLARE_MESSAGE_MAP()
public:
	NOTIFYICONDATA m_nid;
	CString m_strWWWRoot;

	afx_msg void OnDestroy();

	afx_msg void OnBnClickedButtonSelectDirectory();
	afx_msg void OnBnClickedButtonRunning();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonAbout();
	afx_msg void OnBnClickedButtonHide();
	afx_msg void OnBnClickedButtonExit();
	HANDLE m_hRunningThread;
	DWORD m_dwThreadId;
	CButton m_ctlRunningButton;
	CButton m_ctlStopButton;
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};
