#include "Viewer.hpp"
#include <X11/Xutil.h>
#include <cmath>
#include <iostream>
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
		vs.scrollbar = std::make_unique<Scrollbar>(5, toolbar_height_, 500, 20);
		vs.xy_radio = std::make_unique<RadioButton>(400, 5, "XY");
		vs.xz_radio = std::make_unique<RadioButton>(450, 5, "XZ");
		vs.yz_radio = std::make_unique<RadioButton>(500, 5, "YZ");
		vs.xy_radio->set_selected(true);
		update_scrollbar_range(vs);
		update_canvas_dimensions(vs);

		views_.push_back(std::move(vs));

		size_t curr_view_id = views_.size() - 1;

		views_[curr_view_id].zoom_btn->set_callback(
		    [this, curr_view_id]
		    {
			    views_[curr_view_id].zoom_mode =
			        !views_[curr_view_id].zoom_mode;
			    views_[curr_view_id].drag_mode = false;
			    if (views_[curr_view_id].zoom_mode)
				    views_[curr_view_id].drag_btn->set_pressed(false);
		    });

		views_[curr_view_id].drag_btn->set_callback(
		    [this, curr_view_id]
		    {
			    views_[curr_view_id].drag_mode =
			        !views_[curr_view_id].drag_mode;
			    views_[curr_view_id].zoom_mode = false;
			    if (views_[curr_view_id].drag_mode)
				    views_[curr_view_id].zoom_btn->set_pressed(false);
		    });
	}

	XMapWindow(display_, window_);

	XSetWindowAttributes attrs;
	attrs.backing_store = WhenMapped;
	XChangeWindowAttributes(display_, window_, CWBackingStore, &attrs);

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
		usleep(10000);
	}
}

void Viewer::handle_events()
{
	XEvent event;
	while (XPending(display_))
	{
		XNextEvent(display_, &event);

		bool widget_handled = false;
		for (auto& view : views_)
		{
			// Handle all widgets
			if (view.min_input->handle_event(event))
				widget_handled = true;
			if (view.max_input->handle_event(event))
				widget_handled = true;
			if (view.zoom_btn->handle_event(event))
				widget_handled = true;
			if (view.drag_btn->handle_event(event))
				widget_handled = true;
			if (view.scrollbar->handle_event(event))
			{
				view.current_slice = view.scrollbar->get_value();
				widget_handled = true;
			}

			// Radio buttons
			if (view.xy_radio->handle_event(event))
			{
				view.xy_radio->set_selected(true);
				view.xz_radio->set_selected(false);
				view.yz_radio->set_selected(false);
				view.plane = ViewState::Plane::XY;
				update_scrollbar_range(view);
				update_canvas_dimensions(view);
				widget_handled = true;
			}
			if (view.xz_radio->handle_event(event))
			{
				view.xy_radio->set_selected(false);
				view.xz_radio->set_selected(true);
				view.yz_radio->set_selected(false);
				view.plane = ViewState::Plane::XZ;
				update_scrollbar_range(view);
				update_canvas_dimensions(view);
				widget_handled = true;
			}
			if (view.yz_radio->handle_event(event))
			{
				view.xy_radio->set_selected(false);
				view.xz_radio->set_selected(false);
				view.yz_radio->set_selected(true);
				view.plane = ViewState::Plane::YZ;
				update_scrollbar_range(view);
				update_canvas_dimensions(view);
				widget_handled = true;
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
				interaction_.start_x = event.xbutton.x;
				interaction_.start_y = event.xbutton.y;
				interaction_.current_x = event.xbutton.x;
				interaction_.current_y = event.xbutton.y;

				// Find which view was clicked
				for (size_t i = 0; i < views_.size(); i++)
				{
					if (is_point_in_view(event.xbutton.x, event.xbutton.y,
					                     views_[i]))
					{
						interaction_.active_view = i;
						break;
					}
				}

				auto& active_view = views_[interaction_.active_view];
				if (active_view.zoom_mode)
				{
					interaction_.mode = InteractionState::Mode::ZOOM_RECT;
				}
				else if (active_view.drag_mode)
				{
					interaction_.mode = InteractionState::Mode::DRAGGING;
				}
			}
			break;

		case MotionNotify:
			if (interaction_.mode != InteractionState::Mode::NONE)
			{
				interaction_.current_x = event.xmotion.x;
				interaction_.current_y = event.xmotion.y;
			}
			break;

		case ButtonRelease:
			if (event.xbutton.button == Button1)
			{
				if (interaction_.mode == InteractionState::Mode::ZOOM_RECT)
				{
					handle_zoom();
				}
				if (interaction_.mode == InteractionState::Mode::DRAGGING)
				{
					handle_drag();
				}
				interaction_.mode = InteractionState::Mode::NONE;
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

	// Draw images horizontally
	int x_pos = scrollbar_width_;                   // Initial X position
	int y_base = toolbar_height_ + image_spacing_;  // Start below toolbar

	for (auto& view : views_)
	{
		// Position and draw scrollbar
		view.scrollbar->x_ = x_pos - scrollbar_width_;
		view.scrollbar->y_ = y_base;
		view.scrollbar->height_ = view.canvas_height;
		view.scrollbar->draw(display_, buffer_, gc_);
		view.canvas_x = x_pos;
		view.canvas_y = y_base;

		// Draw the image
		draw_volume(view, x_pos, y_base);

		// Update X position for next image
		x_pos += view.canvas_width + image_spacing_ + scrollbar_width_;
	}

	// Draw toolbar widgets on top
	draw_widgets();

	// Copy buffer to window
	XCopyArea(display_, buffer_, window_, gc_, 0, 0, 800, 600, 0, 0);

	// Draw zoom rectangle if active
	if (interaction_.mode == InteractionState::Mode::ZOOM_RECT)
	{
		XSetForeground(display_, gc_, 0xFF0000);  // Red color
		XSetLineAttributes(display_, gc_, 2, LineSolid, CapButt, JoinMiter);

		int x1 = std::min(interaction_.start_x, interaction_.current_x);
		int y1 = std::min(interaction_.start_y, interaction_.current_y);
		int x2 = std::max(interaction_.start_x, interaction_.current_x);
		int y2 = std::max(interaction_.start_y, interaction_.current_y);

		XDrawRectangle(display_, buffer_, gc_, x1, y1, x2 - x1, y2 - y1);
		XSetLineAttributes(display_, gc_, 1, LineSolid, CapButt, JoinMiter);
	}
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
		view.zoom_btn->x_ = x_pos;
		view.zoom_btn->y_ = view.max_input->height_ + 10;
		view.drag_btn->x_ = x_pos + view.zoom_btn->width_ + 10;
		view.drag_btn->y_ = view.zoom_btn->y_;
		view.xy_radio->x_ = x_pos + 200;
		view.xy_radio->y_ = 8;
		view.xz_radio->x_ = view.drag_btn->x_ + view.drag_btn->width_ + 10;
		view.xz_radio->y_ = view.zoom_btn->y_ + 3;
		view.yz_radio->x_ = view.xz_radio->x_ + 50;
		view.yz_radio->y_ = view.zoom_btn->y_ + 3;

		// Draw widgets
		view.min_input->draw(display_, buffer_, gc_);
		view.max_input->draw(display_, buffer_, gc_);
		view.zoom_btn->draw(display_, buffer_, gc_);
		view.drag_btn->draw(display_, buffer_, gc_);
		view.xy_radio->draw(display_, buffer_, gc_);
		view.xz_radio->draw(display_, buffer_, gc_);
		view.yz_radio->draw(display_, buffer_, gc_);

		x_pos += 500;  // Space for next image's controls
	}
}

void Viewer::draw_volume(const ViewState& view, int x_base, int y_base)
{
	const int w = view.canvas_width;
	const int h = view.canvas_height;

	const float min_value = view.volume->window_min();
	const float max_value = view.volume->window_max();

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			const int img_x =
			    view.volume_x_start + x * view.volume_x_end / view.canvas_width;
			const int img_y = view.volume_y_start +
			                  y * view.volume_y_end / view.canvas_height;

			float val;
			switch (view.plane)
			{
			case ViewState::Plane::XY:
				val = view.volume->at(img_x, img_y, view.current_slice);
				break;
			case ViewState::Plane::XZ:
				val = view.volume->at(img_x, view.current_slice, img_y);
				break;
			case ViewState::Plane::YZ:
				val = view.volume->at(view.current_slice, img_x, img_y);
				break;
			}

			uint8_t intensity;
			if (val <= min_value)
			{
				intensity = 0u;
			}
			else if (val >= max_value)
			{
				intensity = 255u;
			}
			else
			{
				intensity = static_cast<uint8_t>(255 * (val - min_value) /
				                                 (max_value - min_value));
			}

			XSetForeground(display_, gc_,
			               intensity << 16 | intensity << 8 | intensity);

			const int screen_x = x_base + x;
			const int screen_y = y_base + y;

			if (screen_x >= 0 && screen_x < 800 &&
			    screen_y >= toolbar_height_ && screen_y < 600)
			{
				XFillRectangle(display_, buffer_, gc_, screen_x, screen_y, 1,
				               1);
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

// Handle dragging
void Viewer::handle_drag()
{
	auto& view = views_[interaction_.active_view];
	const int curr_volume_view_size_x = view.volume_x_end - view.volume_x_start;
	const int curr_volume_view_size_y = view.volume_y_end - view.volume_y_start;
	const int delta_x = curr_volume_view_size_x *
	                    (interaction_.start_x - interaction_.current_x) /
	                    view.canvas_width;
	const int delta_y = curr_volume_view_size_y *
	                    (interaction_.start_y - interaction_.current_y) /
	                    view.canvas_height;
	view.volume_x_start += delta_x;
	view.volume_x_end += delta_x + 1;
	view.volume_y_start += delta_y;
	view.volume_y_end += delta_y + 1;
}

// Handle zoom rectangle
void Viewer::handle_zoom()
{
	auto& view = views_[interaction_.active_view];

	// Convert screen coordinates to image coordinates
	const int curr_volume_view_size_x = view.volume_x_end - view.volume_x_start;
	const int curr_volume_view_size_y = view.volume_y_end - view.volume_y_start;
	const int img_x1 = curr_volume_view_size_x *
	                   (interaction_.start_x - view.canvas_x) /
	                   view.canvas_width;
	const int img_y1 = curr_volume_view_size_y *
	                   (interaction_.start_y - view.canvas_y) /
	                   view.canvas_height;
	const int img_x2 = curr_volume_view_size_x *
	                   (interaction_.current_x - view.canvas_x) /
	                   view.canvas_width;
	const int img_y2 = curr_volume_view_size_y *
	                   (interaction_.current_y - view.canvas_y) /
	                   view.canvas_height;

	// Calculate zoom area
	int rect_width = abs(img_x2 - img_x1);
	int rect_height = abs(img_y2 - img_y1);

	if (rect_width > 5 && rect_height > 5)
	{
		// Calculate new zoom
		view.volume_x_start = std::min(img_x1, img_x2);
		view.volume_x_end = std::max(img_x1, img_x2);
		view.volume_y_start = std::min(img_y1, img_y2);
		view.volume_y_end = std::max(img_y1, img_y2);
	}
}

// New helper functions
void Viewer::update_scrollbar_range(ViewState& view)
{
	switch (view.plane)
	{
	case ViewState::Plane::XY:
		view.scrollbar->set_range(0, view.volume->nz() - 1);
		break;
	case ViewState::Plane::XZ:
		view.scrollbar->set_range(0, view.volume->ny() - 1);
		break;
	case ViewState::Plane::YZ:
		view.scrollbar->set_range(0, view.volume->nx() - 1);
		break;
	}
}

void Viewer::update_canvas_dimensions(ViewState& view)
{
	switch (view.plane)
	{
	case ViewState::Plane::XY:
		view.canvas_width = view.volume->nx();
		view.canvas_height = view.volume->ny();
		break;
	case ViewState::Plane::XZ:
		view.canvas_width = view.volume->nx();
		view.canvas_height = view.volume->nz();
		break;
	case ViewState::Plane::YZ:
		view.canvas_width = view.volume->ny();
		view.canvas_height = view.volume->nz();
		break;
	}
	view.volume_x_start = 0;
	view.volume_y_start = 0;
	view.volume_x_end = view.canvas_width;
	view.volume_y_end = view.canvas_height;
}

// New helper function
bool Viewer::is_point_in_view(int x, int y, const ViewState& view)
{
	// Calculate view position based on layout
	int view_x = view.scrollbar->x_ + scrollbar_width_ + image_spacing_;
	int view_y = toolbar_height_ + image_spacing_;
	int view_width = get_view_width(view);
	int view_height = get_view_height(view);

	return x >= view_x && x <= view_x + view_width && y >= view_y &&
	       y <= view_y + view_height;
}

int Viewer::get_view_width(const ViewState& view) const
{
	switch (view.plane)
	{
	case ViewState::Plane::XY: return view.volume->nx();
	case ViewState::Plane::XZ: return view.volume->nx();
	case ViewState::Plane::YZ: return view.volume->ny();
	}
}

int Viewer::get_view_height(const ViewState& view) const
{
	switch (view.plane)
	{
	case ViewState::Plane::XY: return view.volume->ny();
	case ViewState::Plane::XZ: return view.volume->nz();
	case ViewState::Plane::YZ: return view.volume->nz();
	}
}
