#include "pch.h"
#include "framework.h"
#include "EventProcessing.Diag.h"
#include "EventProcessingDiagDlg.h"
#include "MetavisionRuntime.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CEventProcessingDiagApp, CWinApp)
END_MESSAGE_MAP()

CEventProcessingDiagApp::CEventProcessingDiagApp()
{
}

CEventProcessingDiagApp theApp;

BOOL CEventProcessingDiagApp::InitInstance()
{
    CWinApp::InitInstance();

#ifdef EVENTCORE_HAVE_METAVISION
    eventcore::EnsureBundledHalPluginPath();
#endif

    AfxEnableControlContainer();

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    CEventProcessingDiagDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    return FALSE;
}
