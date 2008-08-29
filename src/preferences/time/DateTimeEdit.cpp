/*
 * Copyright 2004-2007, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		McCall <mccall@@digitalparadise.co.uk>
 *		Mike Berg <mike@berg-net.us>
 *		Julun <host.haiku@gmx.de>
 *
 */

#include "DateTimeEdit.h"


#include <List.h>
#include <String.h>


using BPrivate::B_LOCAL_TIME;


TTimeEdit::TTimeEdit(BRect frame, const char *name, uint32 sections)
	: TSectionEdit(frame, name, sections)
{
	InitView();
	fTime = BTime::CurrentTime(B_LOCAL_TIME);
}


TTimeEdit::~TTimeEdit()
{
}


void
TTimeEdit::InitView()
{
	// make sure we call the base class method, as it
	// will create the arrow bitmaps and the section list
	TSectionEdit::InitView();
	SetSections(fSectionArea);
}


void
TTimeEdit::DrawSection(uint32 index, bool hasFocus)
{
	// user defined section drawing
	TSection *section = NULL;
	section = static_cast<TSection*> (fSectionList->ItemAt(index));

	if (!section)
		return;

	BRect bounds = section->Frame();
	uint32 value = _SectionValue(index);

	SetLowColor(ViewColor());
	if (hasFocus) {
		SetLowColor(tint_color(ViewColor(), B_DARKEN_1_TINT));
		value = fHoldValue;
	}

	BString text;
	switch (index) {
		case 0: 
		{	// hour
			if (value > 12) {
				if (value < 22)
					text << "0";
				text << value - 12;
			} else if (value == 0) {
				text << "12";
			} else {
				if (value < 10)
					text << "0";
				text << value;
			}			
		}	break;

		case 1:
		case 2:
		{	// minute
			// second
			if (value < 10)
				text << "0";
			text << value;
		}	break;

		case 3:
		{	// am/pm
			value = fTime.Hour();
			if (value >= 12)
				text << "PM";
			else 
				text << "AM";
		}	break;
		
		default:
			return;
			break;
	}

	// calc and center text in section rect
	float width = be_plain_font->StringWidth(text.String());
	
	BPoint offset(-((bounds.Width()- width) / 2.0) -1.0
		, bounds.Height() / 2.0 -6.0);
	
	SetHighColor(0, 0, 0, 255);
	FillRect(bounds, B_SOLID_LOW);
	DrawString(text.String(), bounds.LeftBottom() - offset);	
}


void
TTimeEdit::DrawSeperator(uint32 index)
{
	if (index == 3)
		return;  

	TSection *section = NULL;
	section = static_cast<TSection*> (fSectionList->ItemAt(index));
	
	if (!section)
		return;

	BRect bounds = section->Frame();
	float sepWidth = SeperatorWidth();	

	char* sep = ":";
	if (index == 2)
		sep = "-";

	float width = be_plain_font->StringWidth(sep);
	BPoint offset(-((sepWidth - width) / 2.0) -1.0
		, bounds.Height() / 2.0 -6.0);
	DrawString(sep, bounds.RightBottom() - offset);	
}


void
TTimeEdit::SetSections(BRect area)
{
	// by default divide up the sections evenly
	BRect bounds(area);
	
	float sepWidth = SeperatorWidth();
	
	float sep_2 = ceil(sepWidth / fSectionCount +1);
	float width = bounds.Width() / fSectionCount -sep_2;
	bounds.right = bounds.left + (width -sepWidth / fSectionCount);
		
	for (uint32 idx = 0; idx < fSectionCount; idx++) {
		fSectionList->AddItem(new TSection(bounds));
		
		bounds.left = bounds.right + sepWidth;
		if (idx == fSectionCount -2)
			bounds.right = area.right -1;
		else
			bounds.right = bounds.left + (width - sep_2);
	}
}


float
TTimeEdit::SeperatorWidth() const
{
	return 10.0f;
}


void
TTimeEdit::SectionFocus(uint32 index)
{
	fFocus = index;
	fHoldValue = _SectionValue(index);
	Draw(Bounds());
}


void
TTimeEdit::SetTime(int32 hour, int32 minute, int32 second)
{
	if (fTime.Hour() == hour && fTime.Minute() == minute 
		&& fTime.Second() == second)
		return;

	fTime.SetTime(hour, minute, second);
	Invalidate(Bounds());
}


void
TTimeEdit::DoUpPress()
{
	if (fFocus == -1)
		SectionFocus(0);
	
	// update displayed value
	fHoldValue += 1;
	
	_CheckRange();
	
	// send message to change time
	DispatchMessage();
}

		
void
TTimeEdit::DoDownPress()
{
	if (fFocus == -1)
		SectionFocus(0);

	// update display value
	fHoldValue -= 1;
	
	_CheckRange();
	
	// send message to change time
	DispatchMessage();
}
		
		
void
TTimeEdit::BuildDispatch(BMessage *message)
{
	const char *fields[3] = { "hour", "minute", "second" };
	
	message->AddBool("time", true);
	
	for (int32 index = 0; index < fSectionList->CountItems() -1; ++index) {
		uint32 data = _SectionValue(index);
		
		if (fFocus == index)
			data = fHoldValue;
		
		message->AddInt32(fields[index], data);
	}
}


void
TTimeEdit::_CheckRange()
{
	int32 value = fHoldValue;
	switch (fFocus) {
		case 0:
		{	// hour
			if (value > 23) 
				value = 0;
			else if (value < 0) 
				value = 23;

			fTime.SetTime(value, fTime.Minute(), fTime.Second());
		}	break;

		case 1:
		{	// minute
			if (value> 59)
				value = 0;
			else if (value < 0)
				value = 59;

			fTime.SetTime(fTime.Hour(), value, fTime.Second());
		}	break;

		case 2:
		{	// second
			if (value > 59)
				value = 0;
			else if (value < 0)
				value = 59;

			fTime.SetTime(fTime.Hour(), fTime.Minute(), value);
		}	break;

		case 3:
		{
			value = fTime.Hour();
			if (value < 13)
				value += 12;
			else
				value -= 12;
			if (value == 24)
				value = 0;

			// modify hour value to reflect change in am/ pm
			fTime.SetTime(value, fTime.Minute(), fTime.Second());
		}	break;

		default:
			return;
	}

	fHoldValue = value;
	Invalidate(Bounds());
}


int32
TTimeEdit::_SectionValue(int32 index) const
{
	int32 value;
	switch (index) {
		case 0:
			value = fTime.Hour();
			break;
		
		case 1:
			value = fTime.Minute();
			break;

		case 2:
			value = fTime.Second();
			break;

		default:
			value = 0;
			break;
	}

	return value;
}


//	#pragma mark -


TDateEdit::TDateEdit(BRect frame, const char *name, uint32 sections)
	: TSectionEdit(frame, name, sections)
{
	InitView();
	fDate = BDate::CurrentDate(B_LOCAL_TIME);
}


TDateEdit::~TDateEdit()
{
}


void
TDateEdit::InitView()
{
	// make sure we call the base class method, as it
	// will create the arrow bitmaps and the section list
	TSectionEdit::InitView();
	SetSections(fSectionArea);
}


void
TDateEdit::DrawSection(uint32 index, bool hasFocus)
{
	// user defined section drawing
	TSection *section = NULL;
	section = static_cast<TSection*> (fSectionList->ItemAt(index));

	if (!section)
		return;

	BRect bounds = section->Frame();
	uint32 value = _SectionValue(index);

	SetLowColor(ViewColor());
	if (hasFocus) {
		SetLowColor(tint_color(ViewColor(), B_DARKEN_1_TINT));
		value = fHoldValue;
	}

	BString text;
	if (index != 0) {
		if (value < 10)	text << "0";
		text << value;
	} else
		text.SetTo(fDate.LongMonthName(value));

	// calc and center text in section rect
	float width = StringWidth(text.String());
	BPoint offset(-(bounds.Width() - width) / 2.0 - 1.0
		, (bounds.Height() / 2.0 - 6.0));

	SetHighColor(0, 0, 0, 255);
	FillRect(bounds, B_SOLID_LOW);
	DrawString(text.String(), bounds.LeftBottom() - offset);	
}


void
TDateEdit::DrawSeperator(uint32 index)
{
	if (index == 3)
		return;

	TSection *section = NULL;
	section = static_cast<TSection*> (fSectionList->ItemAt(index));
	BRect bounds = section->Frame();

	float sepWidth = SeperatorWidth();
	float width = be_plain_font->StringWidth("/");

	BPoint offset(-(sepWidth / 2.0 - width / 2.0) -1.0
		, bounds.Height() / 2.0 -6.0);

	SetHighColor(0, 0, 0, 255);
	DrawString("/", bounds.RightBottom() - offset);	
}


void
TDateEdit::SetSections(BRect area)
{
	// create sections
	for (uint32 idx = 0; idx < fSectionCount; idx++)
		fSectionList->AddItem(new TSection(area));

	BRect bounds(area);
	float sepWidth = SeperatorWidth();

	// year
	TSection *section = NULL;
	float width = be_plain_font->StringWidth("0000") +6;
	bounds.right = area.right;
	bounds.left = bounds.right -width;
	section = static_cast<TSection*> (fSectionList->ItemAt(2));
	section->SetFrame(bounds);

	// day
	width = be_plain_font->StringWidth("00") +6;
	bounds.right = bounds.left -sepWidth;
	bounds.left = bounds.right -width;
	section = static_cast<TSection*> (fSectionList->ItemAt(1));
	section->SetFrame(bounds);

	// month
	bounds.right = bounds.left - sepWidth;
	bounds.left = area.left;
	section = static_cast<TSection*> (fSectionList->ItemAt(0));
	section->SetFrame(bounds);
}


float
TDateEdit::SeperatorWidth() const
{
	return 10.0f;
}


void
TDateEdit::SectionFocus(uint32 index)
{
	fFocus = index;
	fHoldValue = _SectionValue(index);
	Draw(Bounds());
}


void
TDateEdit::SetDate(int32 year, int32 month, int32 day)
{
	if (year == fDate.Year() && month == fDate.Month() && day == fDate.Day())
		return;

	fDate.SetDate(year, month, day);
	Invalidate(Bounds());
}


void
TDateEdit::DoUpPress()
{
	if (fFocus == -1)
		SectionFocus(0);

	// update displayed value
	fHoldValue += 1;

	_CheckRange();

	// send message to change Date
	DispatchMessage();
}

		
void
TDateEdit::DoDownPress()
{
	if (fFocus == -1)
		SectionFocus(0);

	// update display value
	fHoldValue -= 1;

	_CheckRange();

	// send message to change Date
	DispatchMessage();
}
		
		
void
TDateEdit::BuildDispatch(BMessage *message)
{
	const char *fields[3] = { "month", "day", "year" };

	message->AddBool("time", false);

	int32 value;
	for (int32 index = 0; index < fSectionList->CountItems(); ++index) {
		value = _SectionValue(index);

		if (index == fFocus)
			value = fHoldValue;

		message->AddInt32(fields[index], value);
	}
}


void
TDateEdit::_CheckRange()
{
	int32 value = fHoldValue;
	
	switch (fFocus) {
		case 0:
		{
			 // month
			if (value > 12)
				value = 1;
			else if (value < 1)
				value = 12;

			fDate.SetDate(fDate.Year(), value, fDate.Day());
		}	break;

		case 1:
		{
			//day
			int32 days = fDate.DaysInMonth();
			if (value > days)
				value = 1;
			else if (value < 1)
				value = days;

			fDate.SetDate(fDate.Year(), fDate.Month(), value);
		}	break;

		case 2:
		{
			//year
			if (value > 2037)
				value = 2037;
			else if (value < 1970)
				value = 1970;

			fDate.SetDate(value, fDate.Month(), fDate.Day());
		}	break;

		default:
			return;
	}

	fHoldValue = value;
	Draw(Bounds());
}


int32
TDateEdit::_SectionValue(int32 index) const
{
	int32 value = 0;
	switch (index) {
		case 0:
			value = fDate.Month();
			break;

		case 1:
			value = fDate.Day();
			break;

		default:
			value = fDate.Year();
			break;
	}

	return value;
}
