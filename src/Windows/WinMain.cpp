/*
	(C) 2016 Gary Sinitsin. See LICENSE file (MIT license).
*/
#include "Window.h"
#include "../Phone.h"

#include <process.h>  //For _beginthreadex
#include <CommCtrl.h>
#include <WinSock2.h>


static unsigned __stdcall ThreadMain(void* phone) throw()
{
	return reinterpret_cast<tincan::Phone*>(phone)->mainLoop();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
	using namespace tincan;

	// Init common controls (see: http://blogs.msdn.com/b/oldnewthing/archive/2005/07/18/439939.aspx)
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex); 

	// Init Winsock
	WSAData winsock;
	if ( WSAStartup(0x0202, &winsock) )
	{
		Window::errorMessage("Could not start network: WSAStartup failed");
		return 1;
	}
	

	int exitCode = 0;
	Phone* phone = NULL;
	Window* window = NULL;

	try
	{
		// Create Phone object, which will run its main loop in the thread created below
		phone = new Phone();

		// Create Window, which will handle output from and send input to Phone, synchronized via Phone.mutex
		Window::registerClass();
		window = new Window(phone, nCmdShow);

		// Allow Window to respond to Phone activity
		phone->setUpdateHandler(window);
	}
	catch (std::exception& ex)
	{
		Window::errorMessage(ex.what());
		exitCode = 1;
	}
	catch (...)
	{
		Window::errorMessage("Unknown exception");
		exitCode = 1;
	}
	
	
	// Start thread for Phone
	HANDLE hthread = (HANDLE)_beginthreadex(NULL, 0, ThreadMain, phone, 0, NULL);
	if (!hthread)
	{
		Window::errorMessage("Could not start thread");
		exitCode = 1;
	}


	// If object creation succeeded, start the main loops
	if (!exitCode)
	{
		// Run message loop
		MSG msg;
		while(GetMessage(&msg, NULL, 0, 0) > 0)
		{
			if (window->getHandle() == NULL || !IsDialogMessage(window->getHandle(), &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		exitCode = msg.wParam;

		// Tell Phone to exit thread
		phone->setCommand(Phone::CMD_EXIT);

		// Wait for thread to exit cleanly (ignore errors)
		WaitForSingleObject(hthread, INFINITE);
		CloseHandle(hthread);
	}


	// Cleanup (ignore errors)
	delete window;
	delete phone;
	WSACleanup();

	return exitCode;
}
