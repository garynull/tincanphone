/*
	(C) 2016 Gary Sinitsin. See LICENSE.txt (MIT license).
*/
#pragma once

#include "PhoneCommon.h"
#include "Phone.h"
#include <gtk/gtk.h>

namespace tincan {


class Window : public UpdateHandler
{
public:
	static void errorMessage(const string& message, GtkWidget* window = NULL);
	
	Window(Phone* phone, GtkApplication* app);
	~Window();

protected:
	enum {
		LOG_UPDATE_MS  = 150,  //How often to pull log messages out of the Phone thread
		LOG_MAX_LINES  = 250,

		DEFAULT_W    = 600,
		DEFAULT_H    = 350,
		MARGIN       = 10,
		SPACE        = 5
	};
	
	// Implement UpdateHandler
	void sendUpdate()
	{
		// Window::onUpdate will be called in Gtk thread loop
		g_idle_add(&onUpdate, this);
	}

	static gboolean onUpdate(void* windowVoid)
	{
		reinterpret_cast<Window*>(windowVoid)->onUpdate();
		
		// Remove this callback until sendUpdate called again
		return FALSE;
	}
	
	static gboolean onLogUpdateTimer(void* windowVoid)
	{
		reinterpret_cast<Window*>(windowVoid)->updateLog();
		
		// Keep timer going
		return TRUE;
	}
	
	void onUpdate();
	
	void updateLog();
	
	static void onCallSignal(GtkWidget*, gpointer windowVoid);
	
	static void onAnswerOrHangupSignal(GtkWidget*, gpointer windowVoid);

protected:
	Phone*          phone;
	GtkApplication* app;
	GtkWidget*      gtkwin;
	GtkWidget*      logview;
	GtkWidget*      addr;
	GtkWidget*      call;
	GtkWidget*      answerHangup;
};


}
