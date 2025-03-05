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
#include "Widgets.hpp"

struct ImageViewState {
	std::shared_ptr<Volume> volume;

	// Widgets
	std::unique_ptr<TextInput> min_input;
	std::unique_ptr<TextInput> max_input;
	std::unique_ptr<Button> zoom_button;
	std::unique_ptr<Button> drag_button;

	// View state
	float zoom = 1.0;
	int pan_x = 0, pan_y = 0;
	bool zoom_mode = false;
	bool drag_mode = false;
	int sel_x1 = 0, sel_y1 = 0, sel_x2 = 0, sel_y2 = 0;
};

class Viewer
{
public:
	explicit Viewer(const std::vector<std::shared_ptr<Volume>>& volumes);
	~Viewer();
	void run();

private:
	void handle_key_press(XKeyEvent& event);
	void handle_resize(XConfigureEvent& event);
	void draw_frame();
	void handle_button_press(XButtonEvent& event);
	void handle_button_release(XButtonEvent& event);
	void handle_motion(XMotionEvent& event);
	void update_colormap();
	uint32_t value_to_color(float value) const;
	int get_max_slice() const;
	void adjust_window_level(float delta);
	void create_layout();
	void update_colormap_from_inputs();
	void draw_ui();
	void handle_events();
	void update_image_view(ImageViewState& view);

	Display* display_;
	Window window_;
	GC gc_;
	Pixmap buffer_;
	XImage* ximage_;
	std::vector<uint8_t> image_buffer_;
    std::vector<ImageViewState> views;

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
		bool dragging = false;
		int drag_start_x = 0;
		int drag_start_y = 0;
		int pan_start_x = 0;
		int pan_start_y = 0;
	} state_;

	// UI state
	int toolbar_height = 100;
	int current_view = 0;
	bool rubber_band_active = false;
};
