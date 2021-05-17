/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */
 //--------------------------------------------------------------------
#ifdef USESVCUI
#include "ISvcUI.h"
#ifdef EXTERNSVCUI
extern nvSvc::ISvcFactory* g_pFactUI;
extern IWindowHandler *    g_pWinHandler;
extern IWindowConsole*     g_pConsole;
extern IWindowLog*         g_pLog;
extern IProgressBar*       g_pProgress;
extern IWindowFolding*     g_pToggleContainer;
extern void addToggleKeyToMFCUI(char c, bool* target, const char* desc);
extern void shutdownMFCUI();
extern void initMFCUIBase(int x=0, int y=600, int w=400, int h=100);
extern void logMFCUI(int level, const char * txt);
extern void flushMFCUIToggle(int key);
#else
nvSvc::ISvcFactory* g_pFactUI       = NULL;
IWindowHandler *    g_pWinHandler   = NULL;
IWindowConsole*     g_pConsole      = NULL;
IWindowLog*         g_pLog          = NULL;
IProgressBar*       g_pProgress     = NULL;

IWindowFolding*   g_pToggleContainer = NULL;
void addToggleKeyToMFCUI(char c, bool* target, const char* desc)
{
    if(!g_pToggleContainer)
        return;
    g_pToggleContainer->UnFold(0);
    g_pWinHandler->VariableBind(g_pWinHandler->CreateCtrlCheck((LPCSTR)c, desc, g_pToggleContainer), target);
    g_pToggleContainer->UnFold();
}

void shutdownMFCUI()
{
    g_pConsole = NULL;
    g_pLog = NULL;
    if(g_pWinHandler) g_pWinHandler->DestroyAll();
    UISERVICE_UNLOAD(g_pFactUI, g_pWinHandler);
}

//------------------------------------------------------------------------------
// Setup the base layout of the UI
// the rest can be done outside, depending on the sample's needs
void initMFCUIBase(int x=0, int y=600, int w=400, int h=100)
{
    UISERVICE_LOAD(g_pFactUI, g_pWinHandler);
    if(g_pWinHandler)
    {
        // a Log window is a line-by-line logging, with possible icons for message levels
        g_pLog= g_pWinHandler->CreateWindowLog("LOG", "Log");
        g_pLog->SetVisible()->SetLocation(x,y)->SetSize(w-(w*30/100),h);
        (g_pToggleContainer = g_pWinHandler->CreateWindowFolding("TOGGLES", "Toggles", NULL))
            ->SetLocation(x+(w*70/100), y)
            ->SetSize(w*30/100, h)
            ->SetVisible();
        // Console is a window in which you can write and capture characters the user typed...
        //g_pConsole = g_pWinHandler->CreateWindowConsole("CONSOLE", "Console");
        //g_pConsole->SetVisible();//->SetLocation(0,m_winSize[1]+32)->SetSize(m_winSize[0],200);
        // Show and update this control when doing long load/computation... for example
        g_pProgress = g_pWinHandler->CreateWindowProgressBar("PROG", "Loading", NULL);
        g_pProgress->SetVisible(0);
    }
}

void logMFCUI(int level, const char * txt)
{
   if(g_pLog)
        g_pLog->AddMessage(level, txt);
}

extern std::map<char, bool*>    g_toggleMap;
void flushMFCUIToggle(int key)
{
    std::map<char, bool*>::iterator it = g_toggleMap.find(key);
    if(it != g_toggleMap.end())
        g_pWinHandler->VariableFlush(it->second);
}
#endif
#endif
