/*
	(C) 2016 Gary Sinitsin. See LICENSE.txt (MIT license).
*/
#include "Window.h"

namespace tincan {


void Window::errorMessage(const string& message, GtkWidget* window/* = NULL*/)
{
	GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window),
											   GTK_DIALOG_MODAL,
											   GTK_MESSAGE_ERROR,
											   GTK_BUTTONS_CLOSE,
											   "%s", message.c_str());
	gtk_window_set_title(GTK_WINDOW(dialog), "Tin Can Phone Error");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

Window::Window(Phone* phone, GtkApplication* app)
: phone(phone), app(app)
{
	gtkwin = gtk_application_window_new(app);
	//gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(gtkwin), false);
	gtk_window_set_title(GTK_WINDOW(gtkwin), "Tin Can Phone");
	gtk_window_set_icon_name(GTK_WINDOW(gtkwin), "call-start");
	gtk_window_set_position(GTK_WINDOW(gtkwin), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(gtkwin), DEFAULT_W, DEFAULT_H);
	gtk_container_set_border_width(GTK_CONTAINER(gtkwin), MARGIN);
	
	// Create log update timer
	g_timeout_add(LOG_UPDATE_MS, &onLogUpdateTimer, this);

	// Create window child widgets
	GtkWidget* mainbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, MARGIN);
	gtk_container_add(GTK_CONTAINER(gtkwin), mainbox);

	// Create log text view
	GtkWidget* scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(mainbox), scrollwin, true, true, 0);
	
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollwin), GTK_SHADOW_IN);
	
	logview = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(scrollwin), logview);
	
	gtk_text_view_set_editable(GTK_TEXT_VIEW(logview), false);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(logview), false);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(logview), SPACE);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(logview), SPACE);

	// Create bottom row of widgets
	GtkContainer* bottom = GTK_CONTAINER(gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL));
	gtk_container_add(GTK_CONTAINER(mainbox), GTK_WIDGET(bottom));
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bottom), GTK_BUTTONBOX_START);
	
	GtkWidget* label = gtk_label_new("IP address to call:");
	gtk_container_add(bottom, label);
	
	addr = gtk_entry_new();
	gtk_container_add(bottom, addr);
	g_signal_connect(addr, "activate", G_CALLBACK(Window::onCallSignal), this);
	gtk_widget_set_sensitive(addr, false);
	
	call = gtk_button_new_with_label("Call");
	g_signal_connect(call, "clicked", G_CALLBACK(Window::onCallSignal), this);
	gtk_container_add(bottom, call);
	gtk_widget_set_sensitive(call, false);
	
	GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_container_add(bottom, sep);
	gtk_button_box_set_child_non_homogeneous(GTK_BUTTON_BOX(bottom), sep, true);
	
	answerHangup = gtk_button_new_with_label("Answer");
	g_signal_connect(answerHangup, "clicked", G_CALLBACK(Window::onAnswerOrHangupSignal), this);
	gtk_container_add(bottom, answerHangup);
	gtk_widget_set_sensitive(answerHangup, false);

	
	gtk_widget_show_all(GTK_WIDGET(gtkwin));
}

Window::~Window()
{
	phone->setUpdateHandler(NULL);
	while (g_source_remove_by_user_data(this))
		continue;
}

void Window::onUpdate()
{
	switch (phone->getState())
	{
	case Phone::STARTING:
		break;
		
	case Phone::HUNGUP:
		gtk_widget_set_sensitive(addr, true);
		gtk_widget_grab_focus(addr);
		gtk_widget_set_sensitive(call, true);
		gtk_widget_set_sensitive(answerHangup, false);
		break;
	
	case Phone::DIALING:
		gtk_widget_set_sensitive(addr, false);
		gtk_widget_set_sensitive(call, false);
		gtk_widget_set_sensitive(answerHangup, true);
		gtk_button_set_label(GTK_BUTTON(answerHangup), "Cancel call");
		gtk_widget_grab_focus(answerHangup);
		break;
	
	case Phone::RINGING:
		gtk_widget_set_sensitive(addr, false);
		gtk_widget_set_sensitive(call, false);
		gtk_widget_set_sensitive(answerHangup, true);
		gtk_button_set_label(GTK_BUTTON(answerHangup), "Answer");
		gtk_widget_grab_focus(answerHangup);
		break;
	
	case Phone::LIVE:
		gtk_widget_set_sensitive(addr, false);
		gtk_widget_set_sensitive(call, false);
		gtk_widget_set_sensitive(answerHangup, true);
		gtk_button_set_label(GTK_BUTTON(answerHangup), "Hang up");
		gtk_widget_grab_focus(answerHangup);
		break;
	
	case Phone::EXCEPTION:
		gtk_widget_set_sensitive(addr, false);
		gtk_widget_set_sensitive(call, false);
		gtk_widget_set_sensitive(answerHangup, false);
		errorMessage(phone->getErrorMessage(), gtkwin);
		g_application_quit(G_APPLICATION(app));
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
		GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(logview));
		
		// Append logs
		GtkTextIter bufferEnd;
		gtk_text_buffer_get_end_iter(buffer, &bufferEnd);
		gtk_text_buffer_insert(buffer, &bufferEnd, logs.c_str(), logs.size());
		
		// Remove old log lines
		while (gtk_text_buffer_get_line_count(buffer) > LOG_MAX_LINES)
		{
			GtkTextIter lineStart, lineEnd;
			gtk_text_buffer_get_iter_at_line(buffer, &lineStart, 0);
			gtk_text_buffer_get_iter_at_line(buffer, &lineEnd, 1);
			gtk_text_buffer_delete(buffer, &lineStart, &lineEnd);
		}
		
		// Scroll to bottom
		gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(logview), gtk_text_buffer_get_insert(buffer));
	}
}

void Window::onCallSignal(GtkWidget*, gpointer windowVoid)
{
	Window* window = reinterpret_cast<Window*>(windowVoid);
	window->phone->setCommand(Phone::CMD_CALL, gtk_entry_get_text(GTK_ENTRY(window->addr)));
}

void Window::onAnswerOrHangupSignal(GtkWidget*, gpointer windowVoid)
{
	Phone* phone = reinterpret_cast<Window*>(windowVoid)->phone;
	if (phone->getState() == Phone::RINGING)
		phone->setCommand(Phone::CMD_ANSWER);
	else
		phone->setCommand(Phone::CMD_HANGUP);
}
	
}
