/*
	(C) 2016 Gary Sinitsin. See LICENSE.txt (MIT license).
*/
#pragma once

#include "../Phone.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <CommCtrl.h> //For SetWindowSubclass

namespace tincan {


class Window : public UpdateHandler
{
public:
	static void errorMessage(const string& message)
	{
		HWND hWnd = (sWindow) ? sWindow->handle : NULL;
		MessageBoxA(hWnd, message.c_str(), "Tin Can Phone Error", MB_OK | MB_ICONERROR);
	}

	static void registerClass();

	HWND getHandle() const  {return handle;}
	
	Window(Phone* phone, int nCmdShow);
	~Window();
	
protected:
	enum {
		WM_PHONE_UPDATE   = WM_APP+101,
		IDC_ADDR_LABEL    = 101,
		IDC_ADDR          = 102,
		IDC_CALL          = IDOK,
		IDC_ANSWER_HANGUP = 200,
		
		LOG_TIMER_ID   = 1,
		LOG_UPDATE_MS  = 150,  //How often to pull log messages out of the Phone thread
		LOG_MAX_SIZE   = 2000, //How many characters to keep in the log text box

		WIN_MIN_W    = 600,
		WIN_MIN_H    = 350,
		MARGIN       = 10,
		SPACE        = 5,
		BUTTON_W     = 100,
		BUTTON_H     = 25,
		TEXT_H       = 20,
		ADDR_LABEL_W = 100,
		ADDR_W       = 160
	};

	static Window* sWindow;
	Phone* phone;
	HWND   handle;
	int    width;
	int    height;
	HWND   hlog;
	HWND   haddr;

	// Implement UpdateHandler
	void sendUpdate()
	{
		if (handle)
			PostMessage(handle, WM_PHONE_UPDATE, 0, 0);
	}

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static void CALLBACK LogUpdateTimerProc(HWND hWnd, UINT msg, UINT_PTR idEvent, DWORD time)
	{
		assert(sWindow);
		sWindow->updateLog();
	}

	static LRESULT CALLBACK LogWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR);

	void onCreate();

	void onSize();

	void onUpdate();

	void updateLog();

	void onCommand(const WORD id);

	void destroy();

	void setupControl(HWND hctl)
	{
		if (!hctl)
			throw std::runtime_error("Could not create control");

		static HFONT hfDefault = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		SendMessage(hctl, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE, 0));
	}
};


}
