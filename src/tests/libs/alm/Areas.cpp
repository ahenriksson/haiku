#include <Application.h>
#include <Button.h>
#include <List.h>
#include <Window.h>

#include "ALMLayout.h"


class AreasWindow : public BWindow {
public:
	AreasWindow(BRect frame) 
		:
		BWindow(frame, "ALM Areas", B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE)
	{
		button1 = new BButton("1");
		button2 = new BButton("2");
		button3 = new BButton("3");
		button4 = new BButton("4");

		button1->SetExplicitMinSize(BSize(0, 0));
		button1->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
		//button1->SetExplicitAlignment(
			//BAlignment(B_ALIGN_USE_FULL_WIDTH, B_ALIGN_USE_FULL_HEIGHT));

		// create a new BALMLayout and use  it for this window
		BALMLayout* layout = new BALMLayout();
		SetLayout(layout);

		// create extra tabs
		YTab* y1 = layout->AddYTab();
		YTab* y2 = layout->AddYTab();
		YTab* y3 = layout->AddYTab();

		Area* a1 = layout->AddArea(
			layout->Left(), layout->Top(), 
			layout->Right(), y1,
			button1);
		a1->SetTopInset(10);
		a1->SetLeftInset(10);
		a1->SetRightInset(10);

		layout->AddArea(layout->Left(), y1, layout->Right(), y2, button2);
		button2->SetExplicitAlignment(
			BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));

		Area* a3 = layout->AddArea(layout->Left(), y2, layout->Right(), y3,
			button3);
		button3->SetExplicitAlignment(
			BAlignment(B_ALIGN_HORIZONTAL_CENTER, B_ALIGN_VERTICAL_CENTER));
		a3->HasSameHeightAs(a1);

		layout->AddArea(layout->Left(), y3, layout->Right(), layout->Bottom(),
			button4);
		button4->SetExplicitAlignment(
			BAlignment(B_ALIGN_RIGHT, B_ALIGN_BOTTOM));	
	}
	
private:
	BButton* button1;
	BButton* button2;
	BButton* button3;
	BButton* button4;
};


class Areas : public BApplication {
public:
	Areas() 
		:
		BApplication("application/x-vnd.haiku.Areas") 
	{
		BRect frameRect;
		frameRect.Set(100, 100, 300, 300);
		AreasWindow* window = new AreasWindow(frameRect);
		window->Show();
	}
};


int
main()
{
	Areas app;
	app.Run();
	return 0;
}

