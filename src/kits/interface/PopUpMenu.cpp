/*
 * Copyright 2001-2006, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Marc Flerackers (mflerackers@androme.be)
 *		Stefano Ceccherini (burton666@libero.it)
 */

#include <Application.h>
#include <Looper.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>


struct popup_menu_data 
{
	BPopUpMenu *object;
	BWindow *window;
	BMenuItem *selected;
	
	BPoint where;
	BRect rect;
	
	bool async;
	bool autoInvoke;
	bool startOpened;
	bool useRect;
	
	sem_id lock;
};


BPopUpMenu::BPopUpMenu(const char *title, bool radioMode, bool autoRename,
						menu_layout layout)
	:
	BMenu(title, layout),
	fUseWhere(false),
	fAutoDestruct(false),
	fTrackThread(-1)
{
	if (radioMode)
		SetRadioMode(true);

	if (autoRename)
		SetLabelFromMarked(true);
}


BPopUpMenu::BPopUpMenu(BMessage *archive)
	:
	BMenu(archive),
	fUseWhere(false),
	fAutoDestruct(false),
	fTrackThread(-1)
{
}


BPopUpMenu::~BPopUpMenu()
{
	if (fTrackThread >= 0) {
		status_t status;
		while (wait_for_thread(fTrackThread, &status) == B_INTERRUPTED)
			;		
	}
}


status_t
BPopUpMenu::Archive(BMessage *data, bool deep) const
{
	return BMenu::Archive(data, deep);
}


BArchivable *
BPopUpMenu::Instantiate(BMessage *data)
{
	if (validate_instantiation(data, "BPopUpMenu"))
		return new BPopUpMenu(data);

	return NULL;
}


BMenuItem *
BPopUpMenu::Go(BPoint where, bool delivers_message, bool open_anyway, bool async)
{
	return _go(where, delivers_message, open_anyway, NULL, async);
}


BMenuItem *
BPopUpMenu::Go(BPoint where, bool deliversMessage, bool openAnyway,
	BRect clickToOpen, bool async)
{
	return _go(where, deliversMessage, openAnyway, &clickToOpen, async);
}


void
BPopUpMenu::MessageReceived(BMessage *msg)
{
	BMenu::MessageReceived(msg);
}


void
BPopUpMenu::MouseDown(BPoint point)
{
	BView::MouseDown(point);
}


void
BPopUpMenu::MouseUp(BPoint point)
{
	BView::MouseUp(point);
}


void
BPopUpMenu::MouseMoved(BPoint point, uint32 code, const BMessage *msg)
{
	BView::MouseMoved(point, code, msg);
}


void
BPopUpMenu::AttachedToWindow()
{
	BMenu::AttachedToWindow();
}


void
BPopUpMenu::DetachedFromWindow()
{
	BMenu::DetachedFromWindow();
}


void
BPopUpMenu::FrameMoved(BPoint newPosition)
{
	BMenu::FrameMoved(newPosition);
}


void
BPopUpMenu::FrameResized(float newWidth, float newHeight)
{
	BMenu::FrameResized(newWidth, newHeight);
}


BHandler *
BPopUpMenu::ResolveSpecifier(BMessage *msg, int32 index, BMessage *specifier,
	int32 form, const char *property)
{
	return BMenu::ResolveSpecifier(msg, index, specifier, form, property);
}


status_t
BPopUpMenu::GetSupportedSuites(BMessage *data)
{
	return BMenu::GetSupportedSuites(data);
}


status_t
BPopUpMenu::Perform(perform_code d, void *arg)
{
	return BMenu::Perform(d, arg);
}


void
BPopUpMenu::ResizeToPreferred()
{
	BMenu::ResizeToPreferred();
}


void
BPopUpMenu::GetPreferredSize(float *_width, float *_height)
{
	BMenu::GetPreferredSize(_width, _height);
}


void
BPopUpMenu::MakeFocus(bool state)
{
	BMenu::MakeFocus(state);
}


void
BPopUpMenu::AllAttached()
{
	BMenu::AllAttached();
}


void
BPopUpMenu::AllDetached()
{
	BMenu::AllDetached();
}


void
BPopUpMenu::SetAsyncAutoDestruct(bool state)
{
	fAutoDestruct = state;
}


bool
BPopUpMenu::AsyncAutoDestruct() const
{
	return fAutoDestruct;
}


BPoint
BPopUpMenu::ScreenLocation()
{
	if (fUseWhere)
		return fWhere;
	
	BMenuItem *superItem = Superitem();
	BMenu *superMenu = Supermenu();
	BMenuItem *selectedItem = FindItem(superItem->Label());
	BPoint point = superItem->Frame().LeftTop();

	superMenu->ConvertToScreen(&point);

	if (selectedItem != NULL)
		point.y -= selectedItem->Frame().top;

	return point;
}


//	#pragma mark -
//	private methods


void BPopUpMenu::_ReservedPopUpMenu1() {}
void BPopUpMenu::_ReservedPopUpMenu2() {}
void BPopUpMenu::_ReservedPopUpMenu3() {}


BPopUpMenu &
BPopUpMenu::operator=(const BPopUpMenu &)
{
	return *this;
}


BMenuItem *
BPopUpMenu::_go(BPoint where, bool autoInvoke, bool startOpened,
		BRect *_specialRect, bool async)
{
	BMenuItem *selected = NULL;
	
	// Can't use Window(), as the BPopUpMenu isn't attached
	BLooper *looper = BLooper::LooperForThread(find_thread(NULL));
	BWindow *window = dynamic_cast<BWindow *>(looper);
	
	if (window == NULL)
		return NULL;
	
	popup_menu_data *data = new popup_menu_data;
	sem_id sem = create_sem(0, "window close lock");
	
	data->window = window;

	// Asynchronous menu: we set the BWindow menu's semaphore
	// and let BWindow block when needed
	if (async) {
		_set_menu_sem_(window, sem);
	} 
	
	data->object = this;
	data->autoInvoke = autoInvoke;
	data->useRect = _specialRect != NULL;
	if (_specialRect != NULL)
		data->rect = *_specialRect;
	data->async = async;
	data->where = where;
	data->startOpened = startOpened;
	data->selected = selected;
	data->lock = sem;
	
	// Spawn the tracking thread
	fTrackThread = spawn_thread(entry, "popup", B_NORMAL_PRIORITY, data); 
	
	if (fTrackThread >= 0)
		resume_thread(fTrackThread);
	else {
		// Something went wrong. Cleanup and return NULL
		delete_sem(sem);
		if (async)
			_set_menu_sem_(window, B_BAD_SEM_ID);
		delete data;
		return NULL;
	}

	// Synchronous menu: we block on the sem till
	// the other thread deletes it.
	if (!async) {
		if (window) {
			// TODO: usually it's not a good idea to check for a particular error
			// code. Though here we just want to wait till the semaphore is deleted
			// (it will return B_BAD_SEM_ID in that case), not provide locking or whatever.
			while (acquire_sem_etc(sem, 1, B_TIMEOUT, 50000) != B_BAD_SEM_ID)
				window->UpdateIfNeeded();

		}
		
		status_t unused;
		while (wait_for_thread(fTrackThread, &unused) == B_INTERRUPTED)
			;
		
		selected = data->selected;
		
		delete data;

	}
		
	return selected;
}


int32
BPopUpMenu::entry(void *arg)
{
	popup_menu_data *data = static_cast<popup_menu_data *>(arg);
	BPopUpMenu *menu = data->object;	
	BPoint where = data->where;
	BRect *rect = NULL;
	bool autoInvoke = data->autoInvoke;
	bool startOpened = data->startOpened;
		
	if (data->useRect)
		rect = &data->rect;
	
	data->selected = menu->start_track(where, autoInvoke, startOpened, rect);
	
	// Reset the window menu semaphore	
	if (data->async && data->window)
		_set_menu_sem_(data->window, B_BAD_SEM_ID);
	
	delete_sem(data->lock);
	
	// Commit suicide if needed	
	if (menu->fAutoDestruct)
		delete menu;	
			
	if (data->async)
		delete data;
		
	return 0;
}


BMenuItem *
BPopUpMenu::start_track(BPoint where, bool autoInvoke,
		bool startOpened, BRect *_specialRect)
{
	fWhere = where;
	
	// I know, this doesn't look senseful, but don't be fooled,
	// fUseWhere is used in ScreenLocation(), which is a virtual
	// called by BMenu::Track()
	fUseWhere = true;
	
	// Show the menu's window
	Show();
	
	// Wait some time then track the menu
	snooze(50000);
	BMenuItem *result = Track(startOpened, _specialRect);
	if (result != NULL && autoInvoke)
		result->Invoke();
	
	fUseWhere = false;
	
	Hide();
	
	be_app->ShowCursor();

	fTrackThread = -1;

	return result;
}

