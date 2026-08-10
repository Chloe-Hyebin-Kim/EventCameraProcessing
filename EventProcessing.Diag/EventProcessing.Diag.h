#pragma once

#ifndef __AFXWIN_H__
#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"

class CEventProcessingDiagApp : public CWinApp
{
public:
    CEventProcessingDiagApp();

public:
    virtual BOOL InitInstance();

    DECLARE_MESSAGE_MAP()
};

extern CEventProcessingDiagApp theApp;
