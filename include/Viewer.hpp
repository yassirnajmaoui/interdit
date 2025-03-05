// Viewer.hpp
#pragma once

// Include C++ standard headers first
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

class Viewer {
public:
	explicit Viewer(const std::vector<std::shared_ptr<Volume>>& volumes);
	~Viewer();
	void run();

private:
	struct ViewState {
		std::shared_ptr<Volume> volume;
		std::unique_ptr<TextInput> min_input;
		std::unique_ptr<TextInput> max_input;
		std::unique_ptr<Button> zoom_btn;
		std::unique_ptr<Button> drag_btn;
		float zoom = 1.0;
		int pan_x = 0, pan_y = 0;
		bool zoom_mode = false;
		bool drag_mode = false;
	};

	void create_ui();
	void handle_events();
	void draw_ui();
	void draw_widgets();
	void update_colormap();
	void handle_drag();
	void handle_zoom();
	void draw_volume(const ViewState& view, int y_base);

	Display* display_;
	Window window_;
	GC gc_;
	std::vector<ViewState> views_;
	bool running_ = true;

	Pixmap buffer_;
	int toolbar_height_ = 40;
	int view_spacing_ = 10;

	struct InteractionState {
		bool dragging = false;
		int start_x = 0;
		int start_y = 0;
		int current_x = 0;
		int current_y = 0;
	} interaction_;
};
