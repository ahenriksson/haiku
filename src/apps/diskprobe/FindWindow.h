/* 
** Copyright 2004, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
** Distributed under the terms of the OpenBeOS License.
*/
#ifndef FIND_WINDOW_H
#define FIND_WINDOW_H


#include <Window.h>
#include <Messenger.h>

class FindTextView;
class BCheckBox;


class FindWindow : public BWindow {
	public:
		FindWindow(BRect rect, BMessage &previous, BMessenger &target);
		virtual ~FindWindow();

		virtual void WindowActivated(bool active);
		virtual void MessageReceived(BMessage *message);
		virtual bool QuitRequested();

		void SetTarget(BMessenger &target);

	private:
		BMessenger		fTarget;
		FindTextView	*fTextView;
		BCheckBox		*fCaseCheckBox;
		BMenu			*fMenu;
};

#endif	/* FIND_WINDOW_H */
