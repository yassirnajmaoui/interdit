// Viewer.cpp
#include "Viewer.hpp"
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <unistd.h>


Viewer::Viewer(const std::vector<std::shared_ptr<Volume>>& volumes)
    : volumes_(volumes)
{
	display_ = XOpenDisplay(nullptr);
	if (!display_)
		throw std::runtime_error("Cannot open X display");

	int screen = DefaultScreen(display_);
	Window root = RootWindow(display_, screen);

	// Create window
	window_ = XCreateSimpleWindow(display_, root, 0, 0, 800, 600, 1,
	                              BlackPixel(display_, screen),
	                              WhitePixel(display_, screen));

	// Create graphics context
	XGCValues gcv;
	gcv.foreground = BlackPixel(display_, screen);
	gcv.background = WhitePixel(display_, screen);
	gc_ = XCreateGC(display_, window_, GCForeground | GCBackground, &gcv);

	// Create double buffer
	buffer_ = XCreatePixmap(display_, window_, 800, 600,
	                        DefaultDepth(display_, screen));

	// Allocate image buffer
	image_buffer_.resize(800 * 600 * 4);  // 32-bit RGBA
	ximage_ = XCreateImage(display_, DefaultVisual(display_, screen),
	                       DefaultDepth(display_, screen), ZPixmap, 0,
	                       reinterpret_cast<char*>(image_buffer_.data()), 800,
	                       600, 32, 0);

	XSelectInput(display_, window_,
	             ExposureMask | KeyPressMask | ButtonPressMask |
	                 ButtonReleaseMask | ButtonMotionMask |
	                 StructureNotifyMask);

	XMapWindow(display_, window_);
}

Viewer::~Viewer()
{
	XDestroyImage(ximage_);
	XFreePixmap(display_, buffer_);
	XFreeGC(display_, gc_);
	XDestroyWindow(display_, window_);
	XCloseDisplay(display_);
}

void Viewer::run()
{
	XEvent event;
	bool running = true;

	// Initial draw
	draw_frame();

	while (running)
	{
		handle_events();
		update_colormap_from_inputs();
		draw_frame();

		// Continuous rendering
		draw_frame();
		std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS
	}
}
// Create UI layout
void Viewer::create_layout()
{
	// Create buttons for each view
	const int button_spacing = 10;
	int x = button_spacing;

	for (auto& view : views)
	{
		// Create buttons
		view.zoom_button = std::make_unique<Button>(x, 10, 80, 30, "Zoom");
		view.drag_button = std::make_unique<Button>(x + 90, 10, 80, 30, "Drag");

		// Set as toggle buttons
		view.zoom_button->set_toggle(true);
		view.drag_button->set_toggle(true);

		// Set callbacks
		view.zoom_button->set_callback(
		    [this, &view]
		    {
			    view.zoom_mode = view.zoom_button->is_pressed();
			    view.drag_button->set_pressed(false);
		    });

		view.drag_button->set_callback(
		    [this, &view]
		    {
			    view.drag_mode = view.drag_button->is_pressed();
			    view.zoom_button->set_pressed(false);
		    });

		x += 180 + button_spacing;
	}
}

void Viewer::update_colormap_from_inputs()
{
	for (auto& view : views)
	{
		try
		{
			float min = std::stof(view.min_input->get_text());
			float max = std::stof(view.max_input->get_text());
			view.volume->set_window(min, max);
		}
		catch (...)
		{
			// Handle invalid input
		}
	}
}

// Handle events
void Viewer::handle_events()
{
	XEvent event;
	while (XPending(display_))
	{
		XNextEvent(display_, &event);

		// Handle widgets first
		for (auto& view : views)
		{
			if (view.min_input->handle_event(event) ||
			    view.max_input->handle_event(event) ||
			    view.zoom_button->handle_event(event) ||
			    view.drag_button->handle_event(event))
			{
				return;
			}
		}

		// Handle canvas events
		switch (event.type)
		{
		case ButtonPress:
			if (event.xbutton.button == Button1)
			{
				// Start rubber band selection
				if (views[current_view].zoom_mode)
				{
					views[current_view].sel_x1 = event.xbutton.x;
					views[current_view].sel_y1 = event.xbutton.y;
					rubber_band_active = true;
				}
			}
			break;

		case MotionNotify:
			if (rubber_band_active)
			{
				views[current_view].sel_x2 = event.xmotion.x;
				views[current_view].sel_y2 = event.xmotion.y;
			}
			break;

		case ButtonRelease:
			if (event.xbutton.button == Button1 && rubber_band_active)
			{
				// Calculate zoom area
				auto& view = views[current_view];
				int w = view.sel_x2 - view.sel_x1;
				int h = view.sel_y2 - view.sel_y1;
				view.zoom = std::min(view.volume->nx() / float(w),
				                     view.volume->ny() / float(h));
				view.pan_x = view.sel_x1;
				view.pan_y = view.sel_y1;
				rubber_band_active = false;
			}
			break;
		}
	}
}

// Draw rubber band selection
void Viewer::draw_ui()
{
	if (rubber_band_active)
	{
		auto& view = views[current_view];
		XDrawRectangle(display_, window_, gc_, view.sel_x1, view.sel_y1,
		               view.sel_x2 - view.sel_x1, view.sel_y2 - view.sel_y1);
	}
}

void Viewer::handle_key_press(XKeyEvent& event)
{
	KeySym key = XLookupKeysym(&event, 0);
	const int step = 1;

	switch (key)
	{
	case XK_Escape: XCloseDisplay(display_); exit(0);

	// Slice navigation
	case XK_Left: state_.slice = std::max(0, state_.slice - step); break;
	case XK_Right:
		state_.slice = std::min(get_max_slice(), state_.slice + step);
		break;

	// Plane switching
	case XK_x: state_.plane = Plane::XY; break;
	case XK_y: state_.plane = Plane::XZ; break;
	case XK_z: state_.plane = Plane::YZ; break;

	// Zoom
	case XK_plus: state_.zoom *= 1.2f; break;
	case XK_minus: state_.zoom = std::max(0.1f, state_.zoom / 1.2f); break;

	// Window level
	case XK_Up: adjust_window_level(0.05f); break;
	case XK_Down: adjust_window_level(-0.05f); break;
	}
}

void Viewer::handle_resize(XConfigureEvent& event)
{
	// Recreate buffers with new size
	XFreePixmap(display_, buffer_);
	buffer_ = XCreatePixmap(display_, window_, event.width, event.height,
	                        DefaultDepth(display_, DefaultScreen(display_)));

	// Reallocate image buffer
	image_buffer_.resize(event.width * event.height * 4);
	ximage_->data = reinterpret_cast<char*>(image_buffer_.data());
	ximage_->width = event.width;
	ximage_->height = event.height;
	ximage_->bytes_per_line = event.width * 4;
}

void Viewer::draw_frame()
{
	if (volumes_.empty())
		return;

	// Clear buffer
	XSetForeground(display_, gc_,
	               WhitePixel(display_, DefaultScreen(display_)));
	XFillRectangle(display_, buffer_, gc_, 0, 0, ximage_->width,
	               ximage_->height);

	// Draw all volumes
	for (const auto& vol : volumes_)
	{
		// Get slice dimensions
		int width, height;
		switch (state_.plane)
		{
		case Plane::XY:
			width = vol->nx();
			height = vol->ny();
			break;
		case Plane::XZ:
			width = vol->nx();
			height = vol->nz();
			break;
		case Plane::YZ:
			width = vol->ny();
			height = vol->nz();
			break;
		}

		// Calculate visible area
		const int view_width = ximage_->width;
		const int view_height = ximage_->height;

		// Render to image buffer
		for (int y = 0; y < view_height; y++)
		{
			for (int x = 0; x < view_width; x++)
			{
				// Convert screen coordinates to volume coordinates
				const int vol_x =
				    static_cast<int>((x - state_.pan_x) / state_.zoom);
				const int vol_y =
				    static_cast<int>((y - state_.pan_y) / state_.zoom);

				float value = 0.0f;
				if (vol_x >= 0 && vol_x < width && vol_y >= 0 && vol_y < height)
				{
					switch (state_.plane)
					{
					case Plane::XY:
						value = vol->at(vol_x, vol_y, state_.slice);
						break;
					case Plane::XZ:
						value = vol->at(vol_x, state_.slice, vol_y);
						break;
					case Plane::YZ:
						value = vol->at(state_.slice, vol_x, vol_y);
						break;
					}
				}

				// Convert to color (simple grayscale)
				const uint8_t intensity = static_cast<uint8_t>(
				    255 * (value - vol->window_min()) /
				    (vol->window_max() - vol->window_min()));

				const int screen_x =
				    static_cast<int>((x * state_.zoom) + state_.pan_x);
				const int screen_y =
				    static_cast<int>((y * state_.zoom) + state_.pan_y);

				if (screen_x >= 0 && screen_x < ximage_->width &&
				    screen_y >= 0 && screen_y < ximage_->height)
				{
					const int idx = (screen_y * ximage_->width + screen_x) * 4;
					image_buffer_[idx] = intensity;      // Blue
					image_buffer_[idx + 1] = intensity;  // Green
					image_buffer_[idx + 2] = intensity;  // Red
					image_buffer_[idx + 3] = 0xFF;       // Alpha
				}
			}
		}
	}

	// Copy buffer to window
	XPutImage(display_, buffer_, gc_, ximage_, 0, 0, 0, 0, ximage_->width,
	          ximage_->height);
	XCopyArea(display_, buffer_, window_, gc_, 0, 0, ximage_->width,
	          ximage_->height, 0, 0);
}

void Viewer::handle_button_press(XButtonEvent& event)
{
	if (event.button == Button1)
	{  // Left mouse button
		state_.dragging = true;
		state_.drag_start_x = event.x;
		state_.drag_start_y = event.y;
		state_.pan_start_x = state_.pan_x;
		state_.pan_start_y = state_.pan_y;
	}
}

void Viewer::handle_button_release(XButtonEvent& event)
{
	if (event.button == Button1)
	{
		state_.dragging = false;
	}
}

void Viewer::handle_motion(XMotionEvent& event)
{
	if (state_.dragging)
	{
		const int dx = event.x - state_.drag_start_x;
		const int dy = event.y - state_.drag_start_y;
		state_.pan_x = state_.pan_start_x + dx;
		state_.pan_y = state_.pan_start_y + dy;
	}
}

// Helper functions
int Viewer::get_max_slice() const
{
	if (volumes_.empty())
		return 0;
	const auto& vol = volumes_.front();
	switch (state_.plane)
	{
	case Plane::XY: return vol->nz() - 1;
	case Plane::XZ: return vol->ny() - 1;
	case Plane::YZ: return vol->nx() - 1;
	}
	return 0;
}

void Viewer::adjust_window_level(float delta)
{
	if (volumes_.empty())
		return;

	for (auto& vol : volumes_)
	{
		const float range = vol->window_max() - vol->window_min();
		const float new_min = vol->window_min() + delta * range;
		const float new_max = vol->window_max() - delta * range;
		vol->set_window(new_min, new_max);
	}
}
