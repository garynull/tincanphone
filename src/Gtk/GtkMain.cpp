/*
	(C) 2016 Gary Sinitsin. See LICENSE.txt (MIT license).
*/
#include "Window.h"
#include <gtk/gtk.h>

static void* threadMain(void* phone)
{
	int ret = reinterpret_cast<tincan::Phone*>(phone)->mainLoop();
	return reinterpret_cast<void*>(ret);
}

struct MainObjects {
	pthread_t       thread;
	tincan::Phone*  phone;
	tincan::Window* window;
};

static void activateSignal(GtkApplication* app, void* objectsVoid)
{
	using namespace tincan;
	
	MainObjects* objects = reinterpret_cast<MainObjects*>(objectsVoid);
	
	try
	{
		// Create Phone object, which will run its main loop in the thread created below
		objects->phone = new Phone();

		// Create Window, which will handle output from and send input to Phone, synchronized via Phone.mutex
		objects->window = new Window(objects->phone, app);

		// Allow Window to respond to Phone activity
		objects->phone->setUpdateHandler(objects->window);
	}
	catch (std::exception& ex)
	{
		Window::errorMessage(ex.what());
		g_application_quit(G_APPLICATION(app));
		return;
	}
	catch (...)
	{
		Window::errorMessage("Unknown exception");
		g_application_quit(G_APPLICATION(app));
		return;
	}
	
	// Start thread for Phone	
	int error = pthread_create(&objects->thread, NULL, &threadMain, objects->phone);
	if (error)
	{
		Window::errorMessage("Could not start thread");
		g_application_quit(G_APPLICATION(app));
		return;
	}
}

static void shutdownSignal(GtkApplication* app, void* objectsVoid)
{
	using namespace tincan;
	MainObjects* objects = reinterpret_cast<MainObjects*>(objectsVoid);
	if (objects->phone)
		objects->phone->setCommand(Phone::CMD_EXIT);
	pthread_join(objects->thread, NULL);
	delete objects->window;
	delete objects->phone;
}

int main(int argc, char* argv[])
{
	MainObjects objects;
	
	GtkApplication* app = gtk_application_new("com.garysinitsin.tincanphone", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(activateSignal), &objects);
	g_signal_connect(app, "shutdown", G_CALLBACK(shutdownSignal), &objects);
	
	int status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}
