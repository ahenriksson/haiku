//------------------------------------------------------------------------------
//	Copyright (c) 2001-2002, Haiku, Inc.
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		Desktop.cpp
//	Author:			Adi Oanca <adioanca@cotty.iren.ro>
//	Description:	Class used to encapsulate desktop management
//
//------------------------------------------------------------------------------
#include <stdio.h>
#include <Region.h>
#include <Message.h>

#include "AppServer.h"
#include "Desktop.h"
#include "DisplayDriver.h"
#include "DisplayDriverPainter.h"
#include "Globals.h"
#include "Layer.h"
#include "RootLayer.h"
#include "ServerConfig.h"
#include "ServerScreen.h"
#include "ServerApp.h"
#include "ServerWindow.h"
#include "WinBorder.h"
#include "Workspace.h"

//#define DEBUG_DESKTOP

#ifdef DEBUG_DESKTOP
	#define STRACE(a) printf(a)
#else
	#define STRACE(a) /* nothing */
#endif


Desktop::Desktop(void)
{
	fActiveRootLayer = NULL;
	fActiveScreen = NULL;
}


Desktop::~Desktop(void)
{
	for (int32 i = 0; WinBorder *border = (WinBorder *)fWinBorderList.ItemAt(i); i++)
		delete border;

	for (int32 i = 0; RootLayer *rootLayer = (RootLayer *)fRootLayerList.ItemAt(i); i++)
		delete rootLayer;

	for (int32 i = 0; Screen *screen = (Screen *)fScreenList.ItemAt(i); i++)
		delete screen;
}


void
Desktop::Init(void)
{
	DisplayDriver *driver = NULL;

	// Eventually we will loop through drivers until
	// one can't initialize in order to support multiple monitors.
	// For now, we'll just load one and be done with it.
	
	bool initDrivers = true;
	while (initDrivers) {
		driver = new DisplayDriverPainter();
		AddDriver(driver);
		initDrivers = false;
	}

	if (fScreenList.CountItems() < 1) {
		delete this;
		return;
	}

	InitMode();

	SetActiveRootLayerByIndex(0);
}


void
Desktop::AddDriver(DisplayDriver *driver)
{
	if (driver->Initialize()) {
		Screen *screen = new Screen(driver, fScreenList.CountItems() + 1);
			// The driver is now owned by the screen

		// TODO: be careful of screen initialization - monitor may not support 640x480
		screen->SetMode(640, 480, B_RGB32, 60.f);

		fScreenList.AddItem(screen);
	} else {
		driver->Shutdown();
		delete driver;
	}
}


void
Desktop::InitMode(void)
{
	// this is init mode for n-SS.
	fActiveScreen = (Screen *)fScreenList.ItemAt(0);

	for (int32 i = 0; i < fScreenList.CountItems(); i++) {
		Screen *screen = (Screen *)fScreenList.ItemAt(i);

		char name[32];
		sprintf(name, "RootLayer %ld", i + 1);

		RootLayer *rootLayer = new RootLayer(name, 4, this, GetDisplayDriver());
		rootLayer->SetScreens(&screen, 1, 1);
		rootLayer->RunThread();

		fRootLayerList.AddItem(rootLayer);
	}
}


//---------------------------------------------------------------------------
//					Methods for multiple monitors.
//---------------------------------------------------------------------------


inline Screen *
Desktop::ScreenAt(int32 index) const
{
	return static_cast<Screen *>(fScreenList.ItemAt(index));
}


inline int32
Desktop::ScreenCount(void) const
{
	return fScreenList.CountItems();
}


inline Screen *
Desktop::ActiveScreen(void) const
{
	return fActiveScreen;
}


inline void
Desktop::SetActiveRootLayerByIndex(int32 listIndex)
{
	RootLayer *rootLayer = RootLayerAt(listIndex);

	if (rootLayer != NULL)
		SetActiveRootLayer(rootLayer);
}


inline void
Desktop::SetActiveRootLayer(RootLayer *rootLayer)
{
	if (fActiveRootLayer == rootLayer)
		return;

	fActiveRootLayer = rootLayer;
}


RootLayer *
Desktop::ActiveRootLayer(void) const
{
	return fActiveRootLayer;
}


inline int32
Desktop::ActiveRootLayerIndex(void) const
{
	int32 rootLayerCount = CountRootLayers();

	for (int32 i = 0; i < rootLayerCount; i++) {
		if (fActiveRootLayer == (RootLayer *)fRootLayerList.ItemAt(i))
			return i;
	}
	return -1;
}


inline RootLayer *
Desktop::RootLayerAt(int32 index)
{
	return static_cast<RootLayer *>(fRootLayerList.ItemAt(index));
}


inline int32
Desktop::CountRootLayers() const
{
	return fRootLayerList.CountItems();
}


inline DisplayDriver *
Desktop::GetDisplayDriver() const
{
	return ScreenAt(0)->GetDisplayDriver();
}


//---------------------------------------------------------------------------
//				Methods for layer(WinBorder) manipulation.
//---------------------------------------------------------------------------


void
Desktop::AddWinBorder(WinBorder *winBorder)
{
	if (!winBorder)
		return;

	// R2: how to determine the RootLayer to which this window should be added???
	// for now, use ActiveRootLayer() because we only have one instance.

	int32 feel = winBorder->Feel();

	// we are ServerApp thread, we need to lock RootLayer here.
	ActiveRootLayer()->Lock();

	// we're playing with window list. lock first.
	Lock();

	if (fWinBorderList.HasItem(winBorder)) {
		Unlock();
		ActiveRootLayer()->Unlock();
		debugger("AddWinBorder: WinBorder already in Desktop list\n");
		return;
	}

	// we have a new window. store a record of it.
	fWinBorderList.AddItem(winBorder);

	// add FLOATING_APP windows to the local list of all normal windows.
	// This is to keep the order all floating windows (app or subset) when we go from
	// one normal window to another.
	if (feel == B_FLOATING_APP_WINDOW_FEEL || feel == B_NORMAL_WINDOW_FEEL) {
		WinBorder *wb = NULL;
		int32 count = fWinBorderList.CountItems();
		int32 feelToLookFor = (feel == B_NORMAL_WINDOW_FEEL ?
			B_FLOATING_APP_WINDOW_FEEL : B_NORMAL_WINDOW_FEEL);

		for (int32 i = 0; i < count; i++) {
			wb = (WinBorder *)fWinBorderList.ItemAt(i);

			if (wb->App()->ClientTeamID() == winBorder->App()->ClientTeamID()
				&& wb->Feel() == feelToLookFor) {
				// R2: RootLayer comparison is needed.
				feel == B_NORMAL_WINDOW_FEEL ?
					winBorder->fFMWList.AddWinBorder(wb) :
					wb->fFMWList.AddWinBorder(winBorder);
			}
		}
	}

	// add application's list of modal windows.
	if (feel == B_MODAL_APP_WINDOW_FEEL) {
		winBorder->App()->fAppFMWList.AddWinBorder(winBorder);
	}

	// send WinBorder to be added to workspaces
	ActiveRootLayer()->AddWinBorder(winBorder);

	// hey, unlock!
	Unlock();

	ActiveRootLayer()->Unlock();
}


void
Desktop::RemoveWinBorder(WinBorder *winBorder)
{
	if (!winBorder)
		return;

	// we are ServerApp thread, we need to lock RootLayer here.
	ActiveRootLayer()->Lock();

	// we're playing with window list. lock first.
	Lock();

	// remove from main WinBorder list.
	if (fWinBorderList.RemoveItem(winBorder)) {
		int32 feel = winBorder->Feel();

		// floating app/subset and modal_subset windows require special atention because
		// they are/may_be added to the list of a lot normal windows.
		if (feel == B_FLOATING_SUBSET_WINDOW_FEEL
			|| feel == B_MODAL_SUBSET_WINDOW_FEEL
			|| feel == B_FLOATING_APP_WINDOW_FEEL)
		{
			WinBorder *wb = NULL;
			int32 count = fWinBorderList.CountItems();

			for (int32 i = 0; i < count; i++) {
				wb = (WinBorder*)fWinBorderList.ItemAt(i);

				if (wb->Feel() == B_NORMAL_WINDOW_FEEL
					&& wb->App()->ClientTeamID() == winBorder->App()->ClientTeamID()) {
					// R2: RootLayer comparison is needed. We'll see.
					wb->fFMWList.RemoveItem(winBorder);
				}
			}
		}

		// remove from application's list
		if (feel == B_MODAL_APP_WINDOW_FEEL) {
			winBorder->App()->fAppFMWList.RemoveItem(winBorder);
		}
	} else {
		Unlock();
		ActiveRootLayer()->Unlock();
		debugger("RemoveWinBorder: WinBorder not found in Desktop list\n");
		return;
	}

	// Tell to winBorder's RootLayer about this.
	ActiveRootLayer()->RemoveWinBorder(winBorder);

	Unlock();
	ActiveRootLayer()->Unlock();
}


void
Desktop::AddWinBorderToSubset(WinBorder *winBorder, WinBorder *toWinBorder)
{
	// NOTE: we can safely lock the entire method body, because this method is called from
	//		 RootLayer's thread only.

	// we're playing with window list. lock first.
	Lock();

	if (!winBorder || !toWinBorder
		|| !fWinBorderList.HasItem(winBorder)
		|| !fWinBorderList.HasItem(toWinBorder)) {
		Unlock();
		debugger("AddWinBorderToSubset: NULL WinBorder or not found in Desktop list\n");
		return;
	}

	if ((winBorder->Feel() == B_FLOATING_SUBSET_WINDOW_FEEL
			|| winBorder->Feel() == B_MODAL_SUBSET_WINDOW_FEEL)
		&& toWinBorder->Feel() == B_NORMAL_WINDOW_FEEL
		&& toWinBorder->App()->ClientTeamID() == winBorder->App()->ClientTeamID()
		&& !toWinBorder->fFMWList.HasItem(winBorder)) {
		// add to normal_window's list
		toWinBorder->fFMWList.AddWinBorder(winBorder);
	} else {
		Unlock();
		debugger("AddWinBorderToSubset: you must add a subset_window to a normal_window's subset with the same team_id\n");
		return;
	}

	// send WinBorder to be added to workspaces, if not already in there.
	ActiveRootLayer()->AddSubsetWinBorder(winBorder, toWinBorder);

	Unlock();
}


void
Desktop::RemoveWinBorderFromSubset(WinBorder *winBorder, WinBorder *fromWinBorder)
{
	// NOTE: we can safely lock the entire method body, because this method is called from
	//		 RootLayer's thread only.

	// we're playing with window list. lock first.
	Lock();

	if (!winBorder || !fromWinBorder
		|| !fWinBorderList.HasItem(winBorder)
		|| !fWinBorderList.HasItem(fromWinBorder)) {
		Unlock();
		debugger("RemoveWinBorderFromSubset: NULL WinBorder or not found in Desktop list\n");
		return;
	}

	// remove WinBorder from workspace, if needed - some other windows may still have it in their subset
	ActiveRootLayer()->RemoveSubsetWinBorder(winBorder, fromWinBorder);

	if (fromWinBorder->Feel() == B_NORMAL_WINDOW_FEEL) {
		//remove from this normal_window's subset.
		fromWinBorder->fFMWList.RemoveItem(winBorder);
	} else {
		Unlock();
		debugger("RemoveWinBorderFromSubset: you must remove a subset_window from a normal_window's subset\n");
		return;
	}

	Unlock();
}


void
Desktop::SetWinBorderFeel(WinBorder *winBorder, uint32 feel)
{
	// NOTE: this method is called from RootLayer thread only

	// we're playing with window list. lock first.
	Lock();

	RemoveWinBorder(winBorder);
	winBorder->QuietlySetFeel(feel);
	AddWinBorder(winBorder);

	Unlock();
}


WinBorder *
Desktop::FindWinBorderByServerWindowTokenAndTeamID(int32 token, team_id teamID)
{
	WinBorder *wb;

	Lock();
	for (int32 i = 0; (wb = (WinBorder *)fWinBorderList.ItemAt(i)); i++) {
		if (wb->Window()->ClientToken() == token
			&& wb->Window()->ClientTeamID() == teamID)
			break;
	}
	Unlock();

	return wb;
}


//---------------------------------------------------------------------------
//				Methods for various desktop stuff handled by the server
//---------------------------------------------------------------------------


void
Desktop::SetScrollBarInfo(const scroll_bar_info &info)
{
	fScrollBarInfo = info;
}


scroll_bar_info
Desktop::ScrollBarInfo(void) const
{
	return fScrollBarInfo;
}


void
Desktop::SetMenuInfo(const menu_info &info)
{
	fMenuInfo = info;
}


menu_info
Desktop::MenuInfo(void) const
{
	return fMenuInfo;
}


void
Desktop::UseFFMouse(const bool &useffm)
{
	fFFMouseMode = useffm;
}


bool
Desktop::FFMouseInUse(void) const
{
	return fFFMouseMode;
}


void
Desktop::SetFFMouseMode(const mode_mouse &value)
{
	fMouseMode = value;
}


mode_mouse
Desktop::FFMouseMode(void) const
{
	return fMouseMode;
}


void
Desktop::PrintToStream(void)
{
	printf("RootLayer List:\n=======\n");

	for (int32 i = 0; i < fRootLayerList.CountItems(); i++) {
		printf("\t%s\n", ((RootLayer*)fRootLayerList.ItemAt(i))->GetName());
		((RootLayer*)fRootLayerList.ItemAt(i))->PrintToStream();
		printf("-------\n");
	}

	printf("=======\nActive RootLayer: %s\n",
		fActiveRootLayer ? fActiveRootLayer->GetName() : "NULL");
//	printf("Active WinBorder: %s\n", fActiveWinBorder? fActiveWinBorder->Name(): "NULL");

	printf("Screen List:\n");
	for (int32 i = 0; i < fScreenList.CountItems(); i++)
		printf("\t%ld\n", ((Screen*)fScreenList.ItemAt(i))->ScreenNumber());
}


void
Desktop::PrintVisibleInRootLayerNo(int32 no)
{
}
