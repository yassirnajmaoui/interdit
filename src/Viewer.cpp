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

		views_.push_back(std::move(vs));

		size_t curr_view_id = views_.size() - 1;

		views_[curr_view_id].zoom_btn->set_callback(
		    [this, curr_view_id]
		    {
			    views_[curr_view_id].zoom_mode = !views_[curr_view_id].zoom_mode;
			    views_[curr_view_id].drag_mode = false;
			    if (views_[curr_view_id].zoom_mode)
				    views_[curr_view_id].drag_btn->set_pressed(false);
		    });

		views_[curr_view_id].drag_btn->set_callback(
		    [this, curr_view_id]
		    {
			    views_[curr_view_id].drag_mode = !views_[curr_view_id].drag_mode;
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
		usleep(100000);
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
				widget_handled = true;
			}
			if (view.xz_radio->handle_event(event))
			{
				view.xy_radio->set_selected(false);
				view.xz_radio->set_selected(true);
				view.yz_radio->set_selected(false);
				view.plane = ViewState::Plane::XZ;
				update_scrollbar_range(view);
				widget_handled = true;
			}
			if (view.yz_radio->handle_event(event))
			{
				view.xy_radio->set_selected(false);
				view.xz_radio->set_selected(false);
				view.yz_radio->set_selected(true);
				view.plane = ViewState::Plane::YZ;
				update_scrollbar_range(view);
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
					active_view.pan_start_x = active_view.pan_x;
					active_view.pan_start_y = active_view.pan_y;
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
	int x_pos = scrollbar_width_;  // Initial X position
	int y_base = toolbar_height_ + image_spacing_;  // Start below toolbar

	for (auto& view : views_)
	{
		// Calculate image dimensions based on plane
		int img_width, img_height;
		switch (view.plane)
		{
		case ViewState::Plane::XY:
			img_width = view.volume->nx() * view.zoom;
			img_height = view.volume->ny() * view.zoom;
			break;
		case ViewState::Plane::XZ:
			img_width = view.volume->nx() * view.zoom;
			img_height = view.volume->nz() * view.zoom;
			break;
		case ViewState::Plane::YZ:
			img_width = view.volume->ny() * view.zoom;
			img_height = view.volume->nz() * view.zoom;
			break;
		}

		// Position and draw scrollbar
		view.scrollbar->x_ = x_pos - scrollbar_width_;
		view.scrollbar->y_ = y_base;
		view.scrollbar->height_ = img_height;
		view.scrollbar->draw(display_, buffer_, gc_);

		// Draw the image
		draw_volume(view, x_pos, y_base);

		// Update X position for next image
		x_pos += img_width + image_spacing_ + scrollbar_width_;
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
	int w, h;
	switch (view.plane)
	{
	case ViewState::Plane::XY:
		w = view.volume->nx();
		h = view.volume->ny();
		break;
	case ViewState::Plane::XZ:
		w = view.volume->nx();
		h = view.volume->nz();
		break;
	case ViewState::Plane::YZ:
		w = view.volume->ny();
		h = view.volume->nz();
		break;
	}

	const float min_value = view.volume->window_min();
	const float max_value = view.volume->window_max();

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			float val;
			switch (view.plane)
			{
			case ViewState::Plane::XY:
				val = view.volume->at(x, y, view.current_slice);
				break;
			case ViewState::Plane::XZ:
				val = view.volume->at(x, view.current_slice, y);
				break;
			case ViewState::Plane::YZ:
				val = view.volume->at(view.current_slice, x, y);
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

			const int screen_x = x_base + view.pan_x + x * view.zoom;
			const int screen_y = y_base + view.pan_y + y * view.zoom;

			if (screen_x >= 0 && screen_x < 800 &&
			    screen_y >= toolbar_height_ && screen_y < 600)
			{
				XFillRectangle(display_, buffer_, gc_, screen_x, screen_y,
				               view.zoom, view.zoom);
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
	if (interaction_.mode == InteractionState::Mode::DRAGGING)
	{
		view.pan_x =
		    view.pan_start_x + (interaction_.current_x - interaction_.start_x);
		view.pan_y =
		    view.pan_start_y + (interaction_.current_y - interaction_.start_y);
	}
}

// Handle zoom rectangle
void Viewer::handle_zoom()
{
	auto& view = views_[interaction_.active_view];

	// Convert screen coordinates to image coordinates
	int img_x1 = interaction_.start_x - view.pan_x;
	int img_y1 = interaction_.start_y - view.pan_y - toolbar_height_;
	int img_x2 = interaction_.current_x - view.pan_x;
	int img_y2 = interaction_.current_y - view.pan_y - toolbar_height_;

	// Calculate zoom area
	int rect_width = abs(img_x2 - img_x1);
	int rect_height = abs(img_y2 - img_y1);

	if (rect_width > 10 && rect_height > 10)
	{  // Minimum size
		// Calculate new zoom
		float zoom_x = view.volume->nx() / static_cast<float>(rect_width);
		float zoom_y = view.volume->ny() / static_cast<float>(rect_height);
		view.zoom = std::min(zoom_x, zoom_y);

		// Center on selection
		view.pan_x =
		    -(img_x1 + img_x2) / 2 * view.zoom + get_view_width(view) / 2;
		view.pan_y =
		    -(img_y1 + img_y2) / 2 * view.zoom + get_view_height(view) / 2;
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
	case ViewState::Plane::XY: return view.volume->nx() * view.zoom;
	case ViewState::Plane::XZ: return view.volume->nx() * view.zoom;
	case ViewState::Plane::YZ: return view.volume->ny() * view.zoom;
	}
}

int Viewer::get_view_height(const ViewState& view) const
{
	switch (view.plane)
	{
	case ViewState::Plane::XY: return view.volume->ny() * view.zoom;
	case ViewState::Plane::XZ: return view.volume->nz() * view.zoom;
	case ViewState::Plane::YZ: return view.volume->nz() * view.zoom;
	}
}
