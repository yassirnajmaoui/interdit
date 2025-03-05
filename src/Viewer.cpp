// Viewer.cpp
#include "Viewer.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

// Constructor and destructor remain the same...

void Viewer::handle_events(XEvent& event)
{
	switch (event.type)
	{
	case Expose:
		if (event.xexpose.count == 0)
			draw_frame();
		break;

	case KeyPress:
	{
		KeySym key = XLookupKeysym(&event.xkey, 0);
		int step = 1;

		// Handle slice navigation
		if (key == XK_Left)
			state_.slice = std::max(0, state_.slice - step);
		if (key == XK_Right)
			state_.slice = std::min(get_max_slice(), state_.slice + step);

		// Plane switching
		if (key == XK_x)
			state_.plane = Plane::XY;
		if (key == XK_y)
			state_.plane = Plane::XZ;
		if (key == XK_z)
			state_.plane = Plane::YZ;

		// Zoom controls
		if (key == XK_plus)
			state_.zoom *= 1.2;
		if (key == XK_minus)
			state_.zoom = std::max(0.1f, state_.zoom / 1.2f);
		break;
	}

	case ButtonPress:
		// Store initial mouse position for panning
		break;

	case ConfigureNotify:
		// Handle window resize
		XFreePixmap(display_, buffer_);
		buffer_ = XCreatePixmap(
		    display_, window_, event.xconfigure.width, event.xconfigure.height,
		    DefaultDepth(display_, DefaultScreen(display_)));
		break;
	}
}

Viewer::Viewer(const std::vector<std::shared_ptr<Volume>>& volumes)
    : volumes_(volumes)
{
}

Viewer::~Viewer()
{
	// TODO
}

void Viewer::run()
{
	// TODO
}

void Viewer::draw_frame() const
{
	// Clear buffer
	XSetForeground(display_, gc_,
	               WhitePixel(display_, DefaultScreen(display_)));
	XFillRectangle(display_, buffer_, gc_, 0, 0, 800, 600);

	// Draw all volumes
	for (const auto& vol : volumes_)
	{
		// Get current slice dimensions
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

		// Calculate zoomed dimensions
		int zoomed_width = width * state_.zoom;
		int zoomed_height = height * state_.zoom;

		// Draw slice (simplified example)
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				float value = 0.0f;
				switch (state_.plane)
				{
				case Plane::XY: value = vol->at(x, y, state_.slice); break;
				case Plane::XZ: value = vol->at(x, state_.slice, y); break;
				case Plane::YZ: value = vol->at(state_.slice, x, y); break;
				}

				// Convert to color (simple grayscale example)
				uint8_t intensity = static_cast<uint8_t>(
				    255 * (value - vol->window_min()) /
				    (vol->window_max() - vol->window_min()));
				XSetForeground(display_, gc_,
				               intensity << 16 | intensity << 8 | intensity);
				XFillRectangle(
				    display_, buffer_, gc_, state_.pan_x + x * state_.zoom,
				    state_.pan_y + y * state_.zoom, state_.zoom, state_.zoom);
			}
		}
	}

	// Copy buffer to window
	XCopyArea(display_, buffer_, window_, gc_, 0, 0, 800, 600, 0, 0);
}

int Viewer::get_max_slice() const
{
	if (volumes_.empty())
		return 0;
	switch (state_.plane)
	{
	case Plane::XY: return volumes_[0]->nz() - 1;
	case Plane::XZ: return volumes_[0]->ny() - 1;
	case Plane::YZ: return volumes_[0]->nx() - 1;
	}
	return 0;
}
