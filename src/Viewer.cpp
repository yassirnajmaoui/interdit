#include "Viewer.hpp"
#include <X11/Xutil.h>
#include <cmath>
#include <stdexcept>
#include <unistd.h>

Viewer::Viewer(const std::vector<std::shared_ptr<Volume>>& volumes)
{
	display_ = XOpenDisplay(nullptr);
	if (!display_)
		throw std::runtime_error("Cannot open X display");

	int screen = DefaultScreen(display_);
	window_ = XCreateSimpleWindow(display_, RootWindow(display_, screen), 0, 0,
	                              800, 600, 1, BlackPixel(display_, screen),
	                              WhitePixel(display_, screen));
	XStoreName(display_, window_, "Interdit - Volume Viewer");

	gc_ = XCreateGC(display_, window_, 0, nullptr);
	XSelectInput(display_, window_,
	             ExposureMask | KeyPressMask | ButtonPressMask |
	                 ButtonReleaseMask | PointerMotionMask |
	                 StructureNotifyMask);

	// Initialize views
	for (auto& vol : volumes)
	{
		ViewState vs;
		vs.volume = vol;
		vs.min_input = std::make_unique<TextInput>(0, 0, 80, 25);
		vs.max_input = std::make_unique<TextInput>(90, 0, 80, 25);
		vs.zoom_btn = std::make_unique<Button>(180, 0, 60, 25, "Zoom");
		vs.drag_btn = std::make_unique<Button>(250, 0, 60, 25, "Drag");
		views_.push_back(std::move(vs));
	}

	XMapWindow(display_, window_);

	buffer_ = XCreatePixmap(display_, window_, 800, 600,
	                        DefaultDepth(display_, screen));
}

Viewer::~Viewer()
{
	XFreeGC(display_, gc_);
	XDestroyWindow(display_, window_);
	XCloseDisplay(display_);
}

void Viewer::run()
{
	while (running_)
	{
		handle_events();
		update_colormap();
		draw_ui();
		XFlush(display_);
		usleep(100000);
	}
}

void Viewer::handle_events()
{
	XEvent event;
	while (XPending(display_))
	{
		XNextEvent(display_, &event);

		// Handle widgets
		bool widget_handled = false;
		for (auto& view : views_)
		{
			if (view.min_input->handle_event(event) ||
			    view.max_input->handle_event(event) ||
			    view.zoom_btn->handle_event(event) ||
			    view.drag_btn->handle_event(event))
			{
				widget_handled = true;
				break;
			}
		}
		if (widget_handled)
			continue;

		// Handle canvas interactions
		switch (event.type)
		{
		case ButtonPress:
			if (event.xbutton.button == Button1)
			{
				interaction_.dragging = true;
				interaction_.start_x = event.xbutton.x;
				interaction_.start_y = event.xbutton.y;
			}
			break;

		case MotionNotify:
			if (interaction_.dragging)
			{
				interaction_.current_x = event.xmotion.x;
				interaction_.current_y = event.xmotion.y;
				handle_drag();
			}
			break;

		case ButtonRelease:
			if (event.xbutton.button == Button1)
			{
				interaction_.dragging = false;
				handle_zoom();
			}
			break;
		}
	}
}

void Viewer::draw_ui()
{
	// Draw to buffer first
	XSetForeground(display_, gc_,
	               WhitePixel(display_, DefaultScreen(display_)));
	XFillRectangle(display_, buffer_, gc_, 0, 0, 800, 600);

	// Draw images below toolbar
	int y_pos = toolbar_height_;
	for (auto& view : views_)
	{
		draw_volume(view, y_pos);
		y_pos += view.volume->ny() * view.zoom + view_spacing_;
	}

	// Draw widgets on top
	draw_widgets();

	// Copy buffer to window
	XCopyArea(display_, buffer_, window_, gc_, 0, 0, 800, 600, 0, 0);
}

void Viewer::draw_widgets()
{
	int x_pos = 10;
	for (auto& view : views_)
	{
		// Position widgets in toolbar
		view.min_input->x_ = x_pos;
		view.min_input->y_ = 5;
		view.max_input->x_ = x_pos + 100;
		view.max_input->y_ = 5;
		view.zoom_btn->x_ = x_pos + 200;
		view.zoom_btn->y_ = 5;
		view.drag_btn->x_ = x_pos + 280;
		view.drag_btn->y_ = 5;

		// Draw widgets
		view.min_input->draw(display_, buffer_, gc_);
		view.max_input->draw(display_, buffer_, gc_);
		view.zoom_btn->draw(display_, buffer_, gc_);
		view.drag_btn->draw(display_, buffer_, gc_);

		x_pos += 350;
	}
}

void Viewer::draw_volume(const ViewState& view, int y_base)
{
	const float zoom = view.zoom;
	const int pan_x = view.pan_x;
	const int pan_y = view.pan_y;

	for (int y = 0; y < view.volume->ny(); y++)
	{
		for (int x = 0; x < view.volume->nx(); x++)
		{
			const float val = view.volume->at(x, y, 0);
			const uint8_t intensity = static_cast<uint8_t>(
			    255 * (val - view.volume->window_min()) /
			    (view.volume->window_max() - view.volume->window_min()));

			XSetForeground(display_, gc_,
			               intensity << 16 | intensity << 8 | intensity);

			// Apply zoom and pan
			const int screen_x = pan_x + x * zoom;
			const int screen_y = y_base + pan_y + y * zoom;

			if (screen_x >= 0 && screen_x < 800 &&
			    screen_y >= toolbar_height_ && screen_y < 600)
			{
				XFillRectangle(display_, buffer_, gc_, screen_x, screen_y, zoom,
				               zoom);
			}
		}
	}
}

void Viewer::update_colormap()
{
	for (auto& view : views_)
	{
		try
		{
			float min = std::stof(view.min_input->get_text());
			float max = std::stof(view.max_input->get_text());
			view.volume->set_window(min, max);
		}
		catch (...)
		{
			// Invalid input, retain previous values
		}
	}
}

void Viewer::handle_drag()
{
	for (auto& view : views_)
	{
		if (view.drag_btn->is_pressed())
		{
			view.pan_x += interaction_.current_x - interaction_.start_x;
			view.pan_y += interaction_.current_y - interaction_.start_y;
			interaction_.start_x = interaction_.current_x;
			interaction_.start_y = interaction_.current_y;
		}
	}
}

void Viewer::handle_zoom()
{
	for (auto& view : views_)
	{
		if (view.zoom_btn->is_pressed())
		{
			const int dx = interaction_.current_x - interaction_.start_x;
			const int dy = interaction_.current_y - interaction_.start_y;

			if (dx > 10 && dy > 10)
			{  // Minimum zoom area
				const float zoom_x = view.volume->nx() / static_cast<float>(dx);
				const float zoom_y = view.volume->ny() / static_cast<float>(dy);
				view.zoom = std::min(zoom_x, zoom_y);
				view.pan_x = interaction_.start_x;
				view.pan_y = interaction_.start_y - toolbar_height_;
			}
		}
	}
}
