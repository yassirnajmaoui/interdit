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

class Viewer
{
public:
	Viewer(const std::vector<std::shared_ptr<Volume>>& volumes);
	~Viewer();
	void run();

private:
	struct ViewState
	{
		std::shared_ptr<Volume> volume;
		std::unique_ptr<TextInput> min_input;
		std::unique_ptr<TextInput> max_input;
		std::unique_ptr<Button> zoom_btn;
		std::unique_ptr<Button> drag_btn;
		std::unique_ptr<Scrollbar> scrollbar;
		std::unique_ptr<RadioButton> xy_radio;
		std::unique_ptr<RadioButton> xz_radio;
		std::unique_ptr<RadioButton> yz_radio;

		enum class Plane
		{
			XY,
			XZ,
			YZ
		} plane = Plane::XY;
		int current_slice = 0;
		int volume_x_start = 0, volume_x_end = -1;
		int volume_y_start = 0, volume_y_end = -1;
		int canvas_x = 0, canvas_y = 0, canvas_width = -1, canvas_height = -1;
		bool zoom_mode = false;
		bool drag_mode = false;
	};

	void create_ui();
	void handle_events();
	void draw_ui();
	void draw_widgets();
	void update_colormap();
	void handle_drag();
	void draw_volume(const ViewState& view, int x_base, int y_base);
	void handle_zoom();
	void update_scrollbar_range(ViewState& view);
	void update_canvas_dimensions(ViewState& view);
	bool is_point_in_view(int x, int y, const ViewState& view);
	int get_view_width(const ViewState& view) const;
	int get_view_height(const ViewState& view) const;

	Display* display_;
	Window window_;
	GC gc_;
	Pixmap buffer_;
	std::vector<ViewState> views_;
	int toolbar_height_ = 40;
	int view_spacing_ = 20;
	bool running_ = true;
	int image_spacing_ = 30;    // Space between images
	int scrollbar_width_ = 15;  // Width of scrollbars

	struct InteractionState
	{
		enum class Mode
		{
			NONE,
			ZOOM_RECT,
			DRAGGING
		} mode = Mode::NONE;
		int start_x = 0;
		int start_y = 0;
		int current_x = 0;
		int current_y = 0;
		int active_view = 0;
	} interaction_;
};
