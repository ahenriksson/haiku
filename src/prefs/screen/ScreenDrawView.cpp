#include <InterfaceDefs.h>
#include <Message.h>
#include <Roster.h>
#include <Screen.h>
#include <String.h>
#include <View.h>

#include <cstdlib>

#include "Constants.h"
#include "ScreenDrawView.h"

ScreenDrawView::ScreenDrawView(BRect rect, char *name)
	: BView(rect, name, B_FOLLOW_ALL, B_WILL_DRAW)	
{
	BScreen screen(B_MAIN_SCREEN_ID);
	if (!screen.IsValid())
		; //Debugger() ?
		
	desktopColor = screen.DesktopColor(current_workspace());

	display_mode mode;	
	screen.GetMode(&mode);

	fResolution = mode.virtual_width;
}


void
ScreenDrawView::AttachedToWindow()
{
	rgb_color greyColor = { 216, 216, 216, 255 };
	SetViewColor(greyColor);
}


void
ScreenDrawView::MouseDown(BPoint point)
{
	be_roster->Launch("application/x-vnd.Be-BACK");
}


void
ScreenDrawView::Draw(BRect updateRect)
{
	rgb_color red = { 228, 0, 0, 255 };
	rgb_color black = { 0, 0, 0, 255 };
	rgb_color darkColor = {160, 160, 160, 255};
	
	//FIXME: Make the draw code resolution independent
	if (fResolution == 1600) 
	{	
		SetHighColor(darkColor);
	
		FillRoundRect(Bounds(), 3.0, 3.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(Bounds(), 3.0, 3.0);
		
		SetHighColor(desktopColor);
		
		FillRoundRect(BRect(4.0, 4.0, 98.0, 73.0), 2.0, 2.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(4.0, 4.0, 98.0, 73.0), 2.0, 2.0);
		
		SetHighColor(red);
		
		StrokeLine(BPoint(4.0, 75.0), BPoint(5.0, 75.0));
	}
	else if(fResolution == 1280)
	{
		SetHighColor(darkColor);
	
		FillRoundRect(BRect(10.0, 6.0, 92.0, 72.0), 3.0, 3.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(10.0, 6.0, 92.0, 72.0), 3.0, 3.0);
		
		SetHighColor(desktopColor);
		
		FillRoundRect(BRect(14.0, 10.0, 88.0, 68.0), 2.0, 2.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(14.0, 10.0, 88.0, 68.0), 2.0, 2.0);
		
		SetHighColor(red);
		
		StrokeLine(BPoint(15.0, 70.0), BPoint(16.0, 70.0));
	}
	else if(fResolution == 1152)
	{
		SetHighColor(darkColor);
	
		FillRoundRect(BRect(15.0, 9.0, 89.0, 68.0), 3.0, 3.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(15.0, 9.0, 89.0, 68.0), 3.0, 3.0);
		
		SetHighColor(desktopColor);
		
		FillRoundRect(BRect(19.0, 13.0, 85.0, 64.0), 2.0, 2.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(19.0, 13.0, 85.0, 64.0), 2.0, 2.0);
		
		SetHighColor(red);
		
		StrokeLine(BPoint(19.0, 66.0), BPoint(20.0, 66.0));
	}
	else if(fResolution == 1024)
	{
		SetHighColor(darkColor);
	
		FillRoundRect(BRect(18.0, 14.0, 84.0, 64.0), 3.0, 3.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(18.0, 14.0, 84.0, 64.0), 3.0, 3.0);
		
		SetHighColor(desktopColor);
		
		FillRoundRect(BRect(22.0, 18.0, 80.0, 60.0), 2.0, 2.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(22.0, 18.0, 80.0, 60.0), 2.0, 2.0);
		
		SetHighColor(red);
		
		StrokeLine(BPoint(22.0, 62.0), BPoint(23.0, 62.0));
	}
	else if(fResolution == 800)
	{
		SetHighColor(darkColor);
	
		FillRoundRect(BRect(25.0, 19.0, 77.0, 58.0), 3.0, 3.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(25.0, 19.0, 77.0, 58.0), 3.0, 3.0);
		
		SetHighColor(desktopColor);
		
		FillRoundRect(BRect(29.0, 23.0, 73.0, 54.0), 2.0, 2.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(29.0, 23.0, 73.0, 54.0), 2.0, 2.0);
		
		SetHighColor(red);
		
		StrokeLine(BPoint(29.0, 56.0), BPoint(30.0, 56.0));
	}
	else if(fResolution == 640)
	{
		SetHighColor(darkColor);
	
		FillRoundRect(BRect(31.0, 23.0, 73.0, 55.0), 3.0, 3.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(31.0, 23.0, 73.0, 55.0), 3.0, 3.0);
		
		SetHighColor(desktopColor);
		
		FillRoundRect(BRect(35.0, 27.0, 69.0, 51.0), 2.0, 2.0);
		
		SetHighColor(black);
		
		StrokeRoundRect(BRect(35.0, 27.0, 69.0, 51.0), 2.0, 2.0);
		
		SetHighColor(red);
		
		StrokeLine(BPoint(35.0, 53.0), BPoint(36.0, 53.0));
	}
}


void
ScreenDrawView::MessageReceived(BMessage* message)
{
	switch(message->what)
	{
		case UPDATE_DESKTOP_MSG:
		{
			BString resolution;		
			message->FindString("resolution", &resolution);
			
			int32 nextfResolution = atoi(resolution.String());
			
			if (fResolution != nextfResolution)	{
				fResolution = nextfResolution;
				
				Invalidate();
			}
		}
	
		case UPDATE_DESKTOP_COLOR_MSG:
		{
			BScreen screen;
			if (!screen.IsValid())
				break;

			desktopColor = screen.DesktopColor(current_workspace());
			
			Invalidate();
		}
		
		default:
			BView::MessageReceived(message);	
			break;
	}
}
