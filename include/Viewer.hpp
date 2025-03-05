// Viewer.hpp
#pragma once

// Include C++ headers first
#include <cstdint>
#include <memory>
#include <vector>

// Undefine conflicting macros from system headers
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

// Then include X11 headers
extern "C"
{
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
}

#include "Volume.hpp"

class Viewer
{
public:
	explicit Viewer(const std::vector<std::shared_ptr<Volume>>& volumes);
	~Viewer();
	void run();

private:
	void draw_frame() const;
	void handle_events(XEvent& event);
	void update_colormap();
	uint32_t value_to_color(float value) const;
	int get_max_slice() const;

	Display* display_;
	Window window_;
	GC gc_;
	Pixmap buffer_;
	XImage* ximage_;
	std::vector<uint8_t> image_buffer_;

	std::vector<std::shared_ptr<Volume>> volumes_;

	enum class Plane
	{
		XY,
		XZ,
		YZ
	};

	// Visualization state
	struct
	{
		int slice = 0;
		Plane plane = Plane::XY;
		float zoom = 1.0;
		int pan_x = 0;
		int pan_y = 0;
		bool sync_colormap = false;
	} state_;
};
