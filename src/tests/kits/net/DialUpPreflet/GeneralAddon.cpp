//-----------------------------------------------------------------------
//  This software is part of the OpenBeOS distribution and is covered 
//  by the OpenBeOS license.
//
//  Copyright (c) 2003-2004 Waldemar Kornewald, Waldemar.Kornewald@web.de
//-----------------------------------------------------------------------
// GeneralAddon saves the loaded settings.
// GeneralView saves the current settings.
//-----------------------------------------------------------------------

#include "GeneralAddon.h"

#include "InterfaceUtils.h"
#include "MessageDriverSettingsUtils.h"

#include <Box.h>
#include <Button.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>

#include <PPPDefs.h>


#define MSG_SELECT_DEVICE			'SELD'
#define MSG_SELECT_AUTHENTICATOR	'SELA'


#define GENERAL_TAB_AUTHENTICATION	"Authentication"
#define GENERAL_TAB_AUTHENTICATORS	"Authenticators"


GeneralAddon::GeneralAddon(BMessage *addons)
	: DialUpAddon(addons),
	fHasPassword(false),
	fAuthenticatorsCount(0),
	fSettings(NULL),
	fProfile(NULL),
	fGeneralView(NULL)
{
}


GeneralAddon::~GeneralAddon()
{
}


DialUpAddon*
GeneralAddon::FindDevice(const BString& moduleName) const
{
	DialUpAddon *addon;
	for(int32 index = 0; Addons()->FindPointer(DUN_DEVICE_ADDON_TYPE, index,
			reinterpret_cast<void**>(&addon)) == B_OK; index++)
		if(addon && moduleName == addon->KernelModuleName())
			return addon;
	
	return NULL;
}


bool
GeneralAddon::LoadSettings(BMessage *settings, BMessage *profile, bool isNew)
{
	fIsNew = isNew;
	fHasPassword = false;
	fDeviceName = fUsername = fPassword = "";
	fDeviceAddon = NULL;
	fAuthenticatorsCount = 0;
	fSettings = settings;
	fProfile = profile;
	
	if(fGeneralView)
		fGeneralView->Reload();
			// reset all views (empty settings)
	
	if(!settings || !profile || isNew)
		return true;
	
	if(!LoadDeviceSettings())
		return false;
	
	if(!LoadAuthenticationSettings())
		return false;
	
	if(fGeneralView)
		fGeneralView->Reload();
			// reload new settings
	
	return true;
}


bool
GeneralAddon::LoadDeviceSettings()
{
	int32 index = 0;
	BMessage device;
	if(!FindMessageParameter(PPP_DEVICE_KEY, *fSettings, device, &index))
		return false;
			// TODO: tell user that device specification is missing
	
	if(device.FindString(MDSU_VALUES, &fDeviceName) != B_OK)
		return false;
			// TODO: tell user that device specification is missing
	
	device.AddBool(MDSU_VALID, true);
	fSettings->ReplaceMessage(MDSU_PARAMETERS, index, &device);
	
	fDeviceAddon = FindDevice(fDeviceName);
	if(!fDeviceAddon)
		return false;
	
	return fDeviceAddon->LoadSettings(fSettings, fProfile, false);
}


bool
GeneralAddon::LoadAuthenticationSettings()
{
	// we only handle the profile (although settings could contain different data)
	int32 itemIndex = 0;
	BMessage authentication, item;
	
	if(!FindMessageParameter(PPP_AUTHENTICATOR_KEY, *fProfile, item, &itemIndex))
		return true;
	
	// find authenticators (though we load all authenticators, we only use one)
	BString name;
	for(int32 index = 0; item.FindString(MDSU_VALUES, index, &name) == B_OK; index++) {
		BMessage authenticator;
		if(!GetAuthenticator(name, &authenticator))
			return false;
				// fatal error: we do not know how to handle this authenticator
		
		MarkAuthenticatorAsValid(name);
		authentication.AddString(GENERAL_TAB_AUTHENTICATORS, name);
		++fAuthenticatorsCount;
	}
	
	fSettings->AddMessage(GENERAL_TAB_AUTHENTICATION, &authentication);
	
	bool hasUsername = false;
		// a username must be present
	
	// load username and password
	BMessage parameter;
	int32 parameterIndex = 0;
	if(FindMessageParameter("User", item, parameter, &parameterIndex)
			&& parameter.FindString(MDSU_VALUES, &fUsername) == B_OK) {
		hasUsername = true;
		parameter.AddBool(MDSU_VALID, true);
		item.ReplaceMessage(MDSU_PARAMETERS, parameterIndex, &parameter);
	}
	
	parameterIndex = 0;
	if(FindMessageParameter("Password", item, parameter, &parameterIndex)
			&& parameter.FindString(MDSU_VALUES, &fPassword) == B_OK) {
		fHasPassword = true;
		parameter.AddBool(MDSU_VALID, true);
		item.ReplaceMessage(MDSU_PARAMETERS, parameterIndex, &parameter);
	}
	
	// tell DUN whether everything is valid
	if(hasUsername)
		item.AddBool(MDSU_VALID, true);
	
	fProfile->ReplaceMessage(MDSU_PARAMETERS, itemIndex, &item);
	
	return true;
}


bool
GeneralAddon::HasTemporaryProfile() const
{
	return !fGeneralView->DoesSavePassword();
}


void
GeneralAddon::IsModified(bool& settings, bool& profile) const
{
	if(!fSettings) {
		settings = profile = false;
		return;
	}
	
	bool deviceSettings, authenticationSettings, deviceProfile, authenticationProfile;
	
	IsDeviceModified(deviceSettings, deviceProfile);
	IsAuthenticationModified(authenticationSettings, authenticationProfile);
	
	settings = (deviceSettings || authenticationSettings);
	profile = (deviceProfile || authenticationProfile);
}


void
GeneralAddon::IsDeviceModified(bool& settings, bool& profile) const
{
	if(fDeviceAddon)
		settings = (!fGeneralView->DeviceName() || fGeneralView->IsDeviceModified());
	else
		settings = fGeneralView->DeviceName();
	
	profile = false;
}


void
GeneralAddon::IsAuthenticationModified(bool& settings, bool& profile) const
{
	// currently we only support selecting one authenticator
	if(fAuthenticatorsCount == 0)
		settings = fGeneralView->AuthenticatorName();
	else {
		BMessage authentication;
		if(fSettings->FindMessage(GENERAL_TAB_AUTHENTICATION, &authentication) != B_OK) {
			settings = profile = false;
			return;
				// error!
		}
		
		BString authenticator;
		if(authentication.FindString(GENERAL_TAB_AUTHENTICATORS, &authenticator) != B_OK) {
			settings = profile = false;
			return;
				// error!
		}
		
		settings = (!fGeneralView->AuthenticatorName()
			|| authenticator != fGeneralView->AuthenticatorName());
	}
	
	profile = (settings || fUsername != fGeneralView->Username()
		|| (fPassword != fGeneralView->Password() && fHasPassword)
		|| fHasPassword != fGeneralView->DoesSavePassword());
}


bool
GeneralAddon::SaveSettings(BMessage *settings, BMessage *profile, bool saveTemporary)
{
	if(!fSettings || !settings || !fGeneralView->DeviceName())
		return false;
			// TODO: tell user that a device is needed (if we fail because of this)
	
	if(!fDeviceAddon || !fDeviceAddon->SaveSettings(settings, profile, saveTemporary))
		return false;
	
	if(fGeneralView->AuthenticatorName()) {
		BMessage authenticator;
		authenticator.AddString(MDSU_NAME, PPP_AUTHENTICATOR_KEY);
		authenticator.AddString(MDSU_VALUES, fGeneralView->AuthenticatorName());
		settings->AddMessage(MDSU_PARAMETERS, &authenticator);
		
		BMessage username;
		username.AddString(MDSU_NAME, "User");
		username.AddString(MDSU_VALUES, fGeneralView->Username());
		authenticator.AddMessage(MDSU_PARAMETERS, &username);
		
		if(saveTemporary || fGeneralView->DoesSavePassword()) {
			// save password, too
			BMessage password;
			password.AddString(MDSU_NAME, "Password");
			password.AddString(MDSU_VALUES, fGeneralView->Password());
			authenticator.AddMessage(MDSU_PARAMETERS, &password);
		}
		
		profile->AddMessage(MDSU_PARAMETERS, &authenticator);
	}
	
	return true;
}


bool
GeneralAddon::GetPreferredSize(float *width, float *height) const
{
	BRect rect;
	if(Addons()->FindRect(DUN_TAB_VIEW_RECT, &rect) != B_OK)
		rect.Set(0, 0, 200, 300);
			// set default values
	
	if(width)
		*width = rect.Width();
	if(height)
		*height = rect.Height();
	
	return true;
}


BView*
GeneralAddon::CreateView(BPoint leftTop)
{
	if(!fGeneralView) {
		BRect rect;
		Addons()->FindRect(DUN_TAB_VIEW_RECT, &rect);
		fGeneralView = new GeneralView(this, rect);
	}
	
	fGeneralView->MoveTo(leftTop);
	fGeneralView->Reload();
	return fGeneralView;
}


bool
GeneralAddon::GetAuthenticator(const BString& moduleName, BMessage *entry) const
{
	if(!entry)
		return false;
	
	BString name;
	for(int32 index = 0; Addons()->FindMessage(DUN_AUTHENTICATOR_ADDON_TYPE, index,
			entry) == B_OK; index++) {
		entry->FindString("KernelModuleName", &name);
		if(name == moduleName)
			return true;
	}
	
	return false;
}


bool
GeneralAddon::MarkAuthenticatorAsValid(const BString& moduleName)
{
	BMessage authenticator;
	int32 index = 0;
	BString name;
	
	for(; FindMessageParameter(PPP_AUTHENTICATOR_KEY, *fSettings, authenticator,
			&index); index++) {
		authenticator.FindString("KernelModuleName", &name);
		if(name == moduleName) {
			authenticator.AddBool(MDSU_VALID, true);
			fSettings->ReplaceMessage(MDSU_PARAMETERS, index, &authenticator);
			return true;
		}
	}
	
	return false;
}


GeneralView::GeneralView(GeneralAddon *addon, BRect frame)
	: BView(frame, "General", B_FOLLOW_NONE, 0),
	fAddon(addon)
{
	BRect rect = Bounds();
	rect.InsetBy(5, 5);
	rect.bottom = 100;
	fDeviceBox = new BBox(rect, "Device");
	Addon()->Addons()->AddFloat(DUN_DEVICE_VIEW_WIDTH,
		fDeviceBox->Bounds().Width() - 10);
	rect.top = rect.bottom + 10;
	rect.bottom = rect.top
		+ 25 // space for topmost control
		+ 3 * 20 // size of controls
		+ 3 * 5; // space beween controls and bottom of box
	fAuthenticationBox = new BBox(rect, "Authentication");
	
	fDeviceField = new BMenuField(BRect(5, 0, 250, 20), "Device",
		"Device:", new BPopUpMenu("No Devices Found!"));
	fDeviceField->SetDivider(StringWidth(fDeviceField->Label()) + 10);
	fDeviceField->Menu()->SetRadioMode(true);
	AddDevices();
	fDeviceBox->SetLabel(fDeviceField);
	
	fAuthenticatorField = new BMenuField(BRect(5, 0, 250, 20), "Authenticator",
		"Authenticator:", new BPopUpMenu("No Authenticators Found!"));
	fAuthenticatorField->SetDivider(StringWidth(fAuthenticatorField->Label()) + 10);
	fAuthenticatorField->Menu()->SetRadioMode(true);
	AddAuthenticators();
	fAuthenticationBox->SetLabel(fAuthenticatorField);
	
	rect = fAuthenticationBox->Bounds();
	rect.InsetBy(10, 0);
	rect.top = 25;
	rect.bottom = rect.top + 20;
	fUsername = new BTextControl(rect, "username", "Name: ", NULL, NULL);
	rect.top = rect.bottom + 5;
	rect.bottom = rect.top + 20;
	fPassword = new BTextControl(rect, "password", "Password: ", NULL, NULL);
	fPassword->TextView()->HideTyping(true);
	float usernameWidth = StringWidth(fUsername->Label()) + 5;
	float passwordWidth = StringWidth(fPassword->Label()) + 5;
	if(usernameWidth > passwordWidth)
		passwordWidth = usernameWidth;
	else
		usernameWidth = passwordWidth;
	fUsername->SetDivider(usernameWidth);
	fPassword->SetDivider(passwordWidth);
	
	rect.top = rect.bottom + 5;
	rect.bottom = rect.top + 20;
	fSavePassword = new BCheckBox(rect, "SavePassword", "Save Password", NULL);
	
	fAuthenticationBox->AddChild(fUsername);
	fAuthenticationBox->AddChild(fPassword);
	fAuthenticationBox->AddChild(fSavePassword);
	
	AddChild(fDeviceBox);
	AddChild(fAuthenticationBox);
}


GeneralView::~GeneralView()
{
}


void
GeneralView::Reload()
{
	fDeviceAddon = NULL;
	
	BMenuItem *item;
	for(int32 index = 0; index < fDeviceField->Menu()->CountItems(); index++) {
		item = fDeviceField->Menu()->ItemAt(index);
		if(item && item->Message() && item->Message()->FindPointer("Addon",
					reinterpret_cast<void**>(&fDeviceAddon)) == B_OK
				&& fDeviceAddon == Addon()->DeviceAddon())
			break;
	}
	
	if(fDeviceAddon && fDeviceAddon == Addon()->DeviceAddon()) {
		item->SetMarked(true);
		ReloadDeviceView();
	} else {
		fDeviceAddon = NULL;
		item = fDeviceField->Menu()->FindMarked();
		if(item)
			item->SetMarked(false);
	}
	
	if(Addon()->CountAuthenticators() > 0) {
		BString kernelModule, authenticator;
		BMessage authentication;
		if(Addon()->Settings()->FindMessage(GENERAL_TAB_AUTHENTICATION,
				&authentication) == B_OK)
			authentication.FindString(GENERAL_TAB_AUTHENTICATORS, &authenticator);
		BMenu *menu = fAuthenticatorField->Menu();
		for(int32 index = 0; index < menu->CountItems(); index++) {
			item = menu->ItemAt(index);
			if(item && item->Message()
					&& item->Message()->FindString("KernelModuleName",
						&kernelModule) == B_OK && kernelModule == authenticator) {
				item->SetMarked(true);
				break;
			}
		}
	} else
		fAuthenticatorNone->SetMarked(true);
	
	fUsername->SetText(Addon()->Username());
	fPassword->SetText(Addon()->Password());
	fSavePassword->SetValue(Addon()->HasPassword());
	
	ReloadDeviceView();
}


const char*
GeneralView::DeviceName() const
{
	if(fDeviceAddon)
		return fDeviceAddon->KernelModuleName();
	
	return NULL;
}


const char*
GeneralView::AuthenticatorName() const
{
	BMenuItem *marked = fAuthenticatorField->Menu()->FindMarked();
	if(marked && marked != fAuthenticatorNone)
		return marked->Message()->FindString("KernelModuleName");
	
	return NULL;
}


bool
GeneralView::IsDeviceModified() const
{
	if(fDeviceAddon != Addon()->DeviceAddon())
		return true;
	else if(fDeviceAddon) {
		bool settings, profile;
		fDeviceAddon->IsModified(settings, profile);
		return settings;
	}
	
	return false;
}


void
GeneralView::AttachedToWindow()
{
	SetViewColor(Parent()->ViewColor());
	fDeviceField->Menu()->SetTargetForItems(this);
	fAuthenticatorField->Menu()->SetTargetForItems(this);
	fUsername->SetTarget(this);
	fPassword->SetTarget(this);
}


void
GeneralView::MessageReceived(BMessage *message)
{
	switch(message->what) {
		case MSG_SELECT_DEVICE:
			if(message->FindPointer("Addon", reinterpret_cast<void**>(&fDeviceAddon))
					!= B_OK)
				fDeviceAddon = NULL;
			else {
				if(fDeviceAddon != Addon()->DeviceAddon())
					fDeviceAddon->LoadSettings(Addon()->Settings(), Addon()->Profile(),
						Addon()->IsNew());
				
				ReloadDeviceView();
			}
		break;
		
		case MSG_SELECT_AUTHENTICATOR:
		break;
		
		default:
			BView::MessageReceived(message);
	}
}


void
GeneralView::ReloadDeviceView()
{
	// first remove existing device view(s)
	while(fDeviceBox->CountChildren() > 1)
		fDeviceBox->RemoveChild(fDeviceBox->ChildAt(1));
	
	if(!fDeviceAddon)
		return;
	
	BRect rect(fDeviceBox->Bounds());
	float width, height;
	if(!fDeviceAddon->GetPreferredSize(&width, &height)) {
		width = rect.Width();
		height = 50;
	}
	
	if(width > rect.Width())
		width = rect.Width();
	if(height < 10)
		height = 10;
			// set minimum height
	else if(height > 200)
		height = 200;
			// set maximum height
	
	rect.InsetBy((rect.Width() - width) / 2, 0);
		// center horizontally
	rect.top = 25;
	rect.bottom = rect.top + height;
	float deltaY = height + 30 - fDeviceBox->Bounds().Height();
	fDeviceBox->ResizeBy(0, deltaY);
	fAuthenticationBox->MoveBy(0, deltaY);
	
	BView *deviceView = fDeviceAddon->CreateView(rect.LeftTop());
	if(deviceView)
		fDeviceBox->AddChild(deviceView);
}


void
GeneralView::AddDevices()
{
	AddAddonsToMenu(Addon()->Addons(), fDeviceField->Menu(), DUN_DEVICE_ADDON_TYPE,
		MSG_SELECT_DEVICE);
}


void
GeneralView::AddAuthenticators()
{
	fAuthenticatorNone = new BMenuItem("None", new BMessage(MSG_SELECT_AUTHENTICATOR));
	fAuthenticatorField->Menu()->AddItem(fAuthenticatorNone);
	fAuthenticatorNone->SetMarked(true);
	fAuthenticatorField->Menu()->AddSeparatorItem();
	
	BMessage addon;
	for(int32 index = 0;
			Addon()->Addons()->FindMessage(DUN_AUTHENTICATOR_ADDON_TYPE, index,
			&addon) == B_OK; index++) {
		BMessage *message = new BMessage(MSG_SELECT_AUTHENTICATOR);
		message->AddString("KernelModuleName", addon.FindString("KernelModuleName"));
		
		BString name, technicalName, friendlyName;
		bool hasTechnicalName
			= (addon.FindString("TechnicalName", &technicalName) == B_OK);
		bool hasFriendlyName
			= (addon.FindString("FriendlyName", &friendlyName) == B_OK);
		if(hasTechnicalName) {
			name << technicalName;
			if(hasFriendlyName)
				name << " (";
		}
		if(hasFriendlyName) {
			name << friendlyName;
			if(hasTechnicalName)
				name << ")";
		}
		
		int32 insertAt = FindNextMenuInsertionIndex(fAuthenticatorField->Menu(),
			name, 2);
		fAuthenticatorField->Menu()->AddItem(new BMenuItem(name.String(), message),
			insertAt);
	}
}
