/*
 * Copyright 2001-2005, Haiku.
 * Distributed under the terms of the MIT License.
 *
 */
#include "MainWindow.h"

MainWindow::MainWindow(void)
	: BWindow( BRect(100,80,445,343) , "Fonts", B_TITLED_WINDOW, B_NOT_RESIZABLE | 
				B_ASYNCHRONOUS_CONTROLS | B_NOT_ZOOMABLE )
{
	BRect r; 
	BTabView *tabView; 
	BBox *topLevelView;
	double buttonViewHeight = 43.0;
	
	struct font_cache_info cacheInfo;
	int32 screenCacheSize, printCacheSize;
	
	get_font_cache_info(B_SCREEN_FONT_CACHE|B_DEFAULT_CACHE_SETTING, &cacheInfo);
	screenCacheSize = cacheInfo.cache_size >> 10;
	
	get_font_cache_info(B_PRINTING_FONT_CACHE|B_DEFAULT_CACHE_SETTING, &cacheInfo);
	printCacheSize = cacheInfo.cache_size >> 10;
	
	r = Bounds(); 
	r.top += 10;
	r.bottom -= buttonViewHeight+1;
	
	tabView = new BTabView(r, "tabview", B_WIDTH_FROM_LABEL); 
	tabView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR)); 
	
	r = tabView->Bounds(); 
	r.InsetBy(5, 5); 
	r.bottom -= tabView->TabHeight();
	r.right -=2;
	
	fSelectorView = new FontView(r);
	tabView->AddTab(fSelectorView); 
	
	fCacheView = new CacheView(r, 64, 4096, printCacheSize, screenCacheSize);
	tabView->AddTab(fCacheView); 
	
	r = Bounds();
	r.top = r.bottom - buttonViewHeight;
	
	fButtonView = new ButtonView(r);
	
	topLevelView = new BBox(Bounds(), "TopLevelView", B_FOLLOW_ALL, B_WILL_DRAW, B_NO_BORDER);
	
	AddChild(topLevelView);
	topLevelView->AddChild(fButtonView);
	topLevelView->AddChild(tabView);
	
	MoveTo(fSettings.WindowCorner());
}

bool
MainWindow::QuitRequested(void)
{
	fSettings.SetWindowCorner(Frame().LeftTop());

	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}

void
MainWindow::MessageReceived(BMessage *message)
{
	switch(message->what) 
	{
		case M_ENABLE_REVERT:
		{
			fButtonView->SetRevertState(true);
			break;
		}
		case M_RESCAN_FONTS:
		{
			fSelectorView->RescanFonts();
			break;
		}
		case M_SET_DEFAULTS:
		{
			fSelectorView->SetDefaults();
			fCacheView->SetDefaults();
			fButtonView->SetRevertState(true);
			break;
		}
		case M_REVERT:
		{
			fSelectorView->Revert();
			fCacheView->Revert();
			fButtonView->SetRevertState(false);
			break;
		}
		default: 
		{
			BWindow::MessageReceived(message);
			break;
		}
	}
}
