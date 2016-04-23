/*
	(C) 2016 Gary Sinitsin. See LICENSE file (MIT license).
*/
#include "Window.h"

namespace tincan {


Window* Window::sWindow = NULL;

void Window::registerClass()
{
	HINSTANCE hInstance = GetModuleHandle(NULL);

	WNDCLASSEX wc;
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = &Window::WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = L"tincanphoneWnd";
	wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
	wc.hIconSm       = 0;

	if (!RegisterClassEx(&wc))
		throw std::runtime_error("RegisterClassEx failed");
}

Window::Window(Phone* phone, int nCmdShow)
: phone(phone),
  handle(NULL),
  width(WIN_MIN_W),
  height(WIN_MIN_H),
  hlog(NULL),
  haddr(NULL)
{
	if (sWindow)
		throw std::runtime_error("Only one Window can be created");
	sWindow = this;

	CreateWindowEx(
		0,
		L"tincanphoneWnd",
		L"Tin Can Phone",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME,
		GetSystemMetrics(SM_CXSCREEN)/2 - WIN_MIN_W/2, GetSystemMetrics(SM_CYSCREEN)/2 - WIN_MIN_H/2,
		WIN_MIN_W, WIN_MIN_H,
		NULL, NULL, GetModuleHandle(NULL), NULL);

	if (!handle)
		throw std::runtime_error("Could not create window");

	ShowWindow(handle, nCmdShow);
}

Window::~Window()
{
	phone->setUpdateHandler(NULL);
	destroy();
	sWindow = NULL;
}

LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Catch exceptions; they can't be thrown out of WndProc
	try
	{
		switch (msg)
		{
		case WM_CREATE:
			sWindow->handle = hWnd;
			sWindow->onCreate();
			return 0;

		case WM_SIZE:
			// Don't handle message if minimized
			if (wParam == SIZE_MINIMIZED)
				return DefWindowProc(hWnd, msg, wParam, lParam);
			sWindow->width  = LOWORD(lParam);
			sWindow->height = HIWORD(lParam);
			sWindow->onSize();
			// Fixes graphical glitches that happen when resizing
			//InvalidateRect(hWnd, NULL, TRUE);
			return 0;

		case WM_GETMINMAXINFO:
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = WIN_MIN_W;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = WIN_MIN_H;
			return 0;

		case WM_PHONE_UPDATE:
			sWindow->onUpdate();
			return 0;

		case WM_COMMAND:
			sWindow->onCommand(LOWORD(wParam));
			return 0;

		case WM_CLOSE:
			sWindow->destroy();
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
	}
	catch (std::exception& ex)
	{
		errorMessage(ex.what());
		sWindow->destroy();
	}
	catch (...)
	{
		errorMessage("Unknown exception");
		sWindow->destroy();
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK Window::LogWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR)
{
	// Block all keyboard input
	if (msg == WM_KEYDOWN || msg == WM_CHAR || msg == WM_KEYUP)
		return 0;
	else if (msg == WM_NCDESTROY)
		RemoveWindowSubclass(hWnd, LogWndProc, uIdSubclass);
	
	return DefSubclassProc(hWnd, msg, wParam, lParam);
}

void Window::onCreate()
{
	SetTimer(handle, LOG_TIMER_ID, LOG_UPDATE_MS, &LogUpdateTimerProc);

	
	HINSTANCE hInstance = GetModuleHandle(NULL);

	int winh = WIN_MIN_H - 25;

	// Create log control
	hlog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
		WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
		0, 0, WIN_MIN_W, WIN_MIN_H,
		handle, (HMENU)0, hInstance, NULL);
	setupControl(hlog);
	SendMessage(hlog, EM_SETMARGINS, EC_LEFTMARGIN, MARGIN);

	// Subclassing is the simplest way to get the behavior we want - no gray background or weird key/dialog msg handling
	SetWindowSubclass(hlog, &LogWndProc, 0, 0);

	// Create other controls
	HWND hctl;
	hctl = CreateWindow(L"STATIC", L"IP address to call:", WS_VISIBLE | WS_CHILD | SS_RIGHT,
		0, 0, ADDR_LABEL_W, TEXT_H,
		handle, (HMENU)IDC_ADDR_LABEL, hInstance, NULL);
	setupControl(hctl);

	haddr = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL, WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_DISABLED,
		0, 0, ADDR_W, TEXT_H,
		handle, (HMENU)IDC_ADDR, hInstance, NULL);
	setupControl(haddr);

	hctl = CreateWindow(L"BUTTON", L"Call", WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_DISABLED,
		0, 0, BUTTON_W, BUTTON_H,
		handle, (HMENU)IDC_CALL, hInstance, NULL);
	setupControl(hctl);

	hctl = CreateWindow(L"BUTTON", L"Answer", WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_DISABLED,
		0, 0, BUTTON_W, BUTTON_H,
		handle, (HMENU)IDC_ANSWER_HANGUP, hInstance, NULL);
	setupControl(hctl);

	onSize();
}

void Window::onSize()
{
	// Fit log in window, leaving space at the bottom for other controls
	SetWindowPos(hlog, NULL,  1, 1,  width-2, height-MARGIN-BUTTON_H-MARGIN, SWP_NOZORDER);

	// Keep bottom controls at bottom
	const uint nosize = SWP_NOSIZE|SWP_NOZORDER;
	HWND hctl;
	hctl = GetDlgItem(handle, IDC_ADDR_LABEL);
	SetWindowPos(hctl, NULL,  MARGIN, (height-BUTTON_H-MARGIN)+6,  0,0, nosize);

	hctl = GetDlgItem(handle, IDC_ADDR);
	SetWindowPos(hctl, NULL,  MARGIN+ADDR_LABEL_W+SPACE, (height-BUTTON_H-MARGIN)+3,  0,0, nosize);

	hctl = GetDlgItem(handle, IDC_CALL);
	SetWindowPos(hctl, NULL,  MARGIN+ADDR_LABEL_W+SPACE+ADDR_W+SPACE, height-BUTTON_H-MARGIN,  0,0, nosize);

	hctl = GetDlgItem(handle, IDC_ANSWER_HANGUP);
	SetWindowPos(hctl, NULL,  MARGIN+ADDR_LABEL_W+SPACE+ADDR_W+SPACE+BUTTON_W+MARGIN, height-BUTTON_H-MARGIN,  0,0, nosize);
}

void Window::onUpdate()
{
	switch (phone->getState())
	{
	case Phone::STARTING:
		break;

	case Phone::HUNGUP:
		EnableWindow(haddr, TRUE);
		SetFocus(haddr);
		EnableWindow(GetDlgItem(handle, IDC_CALL), TRUE);
		EnableWindow(GetDlgItem(handle, IDC_ANSWER_HANGUP), FALSE);
		break;
	
	case Phone::DIALING:
		EnableWindow(haddr, FALSE);
		EnableWindow(GetDlgItem(handle, IDC_CALL), FALSE);
		EnableWindow(GetDlgItem(handle, IDC_ANSWER_HANGUP), TRUE);
		SetWindowText(GetDlgItem(handle, IDC_ANSWER_HANGUP), L"Cancel call");
		SetFocus(GetDlgItem(handle, IDC_ANSWER_HANGUP));
		break;
	
	case Phone::RINGING:
		EnableWindow(haddr, FALSE);
		EnableWindow(GetDlgItem(handle, IDC_CALL), FALSE);
		EnableWindow(GetDlgItem(handle, IDC_ANSWER_HANGUP), TRUE);
		SetWindowText(GetDlgItem(handle, IDC_ANSWER_HANGUP), L"Answer");
		SetFocus(GetDlgItem(handle, IDC_ANSWER_HANGUP));
		break;
	
	case Phone::LIVE:
		EnableWindow(haddr, FALSE);
		EnableWindow(GetDlgItem(handle, IDC_CALL), FALSE);
		EnableWindow(GetDlgItem(handle, IDC_ANSWER_HANGUP), TRUE);
		SetWindowText(GetDlgItem(handle, IDC_ANSWER_HANGUP), L"Hang up");
		SetFocus(GetDlgItem(handle, IDC_ANSWER_HANGUP));
		break;
	
	case Phone::EXCEPTION:
		EnableWindow(haddr, FALSE);
		EnableWindow(GetDlgItem(handle, IDC_CALL), FALSE);
		EnableWindow(GetDlgItem(handle, IDC_ANSWER_HANGUP), FALSE);
		errorMessage(phone->getErrorMessage());
		destroy();
		return;
	
	case Phone::EXITED:
		return;
	}

	updateLog();
}

void Window::updateLog()
{
	// Pull log messages out of Phone
	string logs = phone->readLog();
	if (!logs.empty())
	{
		// Replace \n with \r\n
		size_t pos = 0;
		while ((pos = logs.find('\n', pos)) != string::npos) {
			logs.replace(pos, 1, "\r\n");
			pos += 2;
		}

		// Remove old logs if necessary
		size_t newloglen = logs.size() + (size_t)GetWindowTextLengthA(hlog);
		if (newloglen > LOG_MAX_SIZE)
		{
			SendMessageA(hlog, EM_SETSEL, 0, newloglen - LOG_MAX_SIZE);
			SendMessageA(hlog, EM_REPLACESEL, FALSE, (LPARAM)"");
		}

		// Append new logs
		SendMessageA(hlog, EM_SETSEL, LOG_MAX_SIZE, LOG_MAX_SIZE);
		SendMessageA(hlog, EM_REPLACESEL, FALSE, (LPARAM)logs.c_str());
	}
}

void Window::onCommand(const WORD id)
{
	if (id == IDC_CALL)
	{
		char addrText[256];
		GetWindowTextA(haddr, addrText, sizeof(addrText));
		phone->setCommand(Phone::CMD_CALL, addrText);
	}
	else if (id == IDC_ANSWER_HANGUP)
	{
		if (phone->getState() == Phone::RINGING)
			phone->setCommand(Phone::CMD_ANSWER);
		else
			phone->setCommand(Phone::CMD_HANGUP);
	}
}

void Window::destroy()
{
	if (handle)
	{
		KillTimer(handle, LOG_TIMER_ID);
		DestroyWindow(handle);
		handle = NULL;
	}
}


}
