#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Float_Input.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <vector>

class Volume
{
public:
	Volume(const std::string& filename, int nx, int ny, int nz)
	    : nx(nx), ny(ny), nz(nz), data(nx * ny * nz)
	{
		std::ifstream file(filename, std::ios::binary);
		file.read(reinterpret_cast<char*>(data.data()),
		          data.size() * sizeof(float));

		auto [min_it, max_it] = std::minmax_element(data.begin(), data.end());
		min_val = *min_it;
		max_val = *max_it;
		win_min = min_val;
		win_max = max_val;
	}

	float get_value(int x, int y, int z) const
	{
		return data[z * nx * ny + y * nx + x];
	}

	int nx, ny, nz;
	float min_val, max_val, win_min, win_max;

private:
	std::vector<float> data;
};

class Canvas : public Fl_Box
{
public:
	Canvas(int X, int Y, int W, int H, Volume& vol)
	    : Fl_Box(X, Y, W, H),
	      vol(vol),
	      zoom(1.0),
	      pan_x(0),
	      pan_y(0),
	      drag_start_x(0),
	      drag_start_y(0),
	      is_dragging(false),
	      mode(Mode::NONE)
	{
	}

	void update_slice(int s)
	{
		current_slice = s;
		redraw();
	}

	void set_orientation(int o)
	{
		orientation = o;
		redraw();
	}

	void set_window(float min, float max)
	{
		vol.win_min = min;
		vol.win_max = max;
		redraw();
	}

	int handle(int event) override
	{
		switch (event)
		{
		case FL_PUSH:
			if (Fl::event_button() == FL_LEFT_MOUSE)
			{
				drag_start_x = Fl::event_x();
				drag_start_y = Fl::event_y();
				is_dragging = true;
				return 1;
			}
			break;

		case FL_DRAG:
			if (is_dragging)
			{
				if (mode == Mode::DRAG)
				{
					pan_x += Fl::event_x() - drag_start_x;
					pan_y += Fl::event_y() - drag_start_y;
					drag_start_x = Fl::event_x();
					drag_start_y = Fl::event_y();
					redraw();
				}
			}
			return 1;

		case FL_RELEASE:
			if (mode == Mode::ZOOM && is_dragging)
			{
				int x1 = std::min(drag_start_x, Fl::event_x());
				int y1 = std::min(drag_start_y, Fl::event_y());
				int x2 = std::max(drag_start_x, Fl::event_x());
				int y2 = std::max(drag_start_y, Fl::event_y());

				float zoom_x = w() / float(x2 - x1);
				float zoom_y = h() / float(y2 - y1);
				zoom = std::min(zoom_x, zoom_y);

				pan_x = -x1 * zoom;
				pan_y = -y1 * zoom;

				redraw();
			}
			is_dragging = false;
			return 1;
		}
		return Fl_Box::handle(event);
	}

	void draw() override
	{
		fl_rectf(x(), y(), w(), h(), FL_WHITE);  // Clear canvas

		// Calculate visible area
		int img_w = vol.nx, img_h = vol.ny;
		if (orientation == 1)
			img_h = vol.nz;  // XZ
		if (orientation == 2)
			img_w = vol.ny;  // YZ

		// Draw image
		for (int iy = 0; iy < h(); iy++)
		{
			for (int ix = 0; ix < w(); ix++)
			{
				int img_x = (ix - pan_x) / zoom;
				int img_y = (iy - pan_y) / zoom;

				if (img_x >= 0 && img_x < img_w && img_y >= 0 && img_y < img_h)
				{
					float val;
					switch (orientation)
					{
					case 0:
						val = vol.get_value(img_x, img_y, current_slice);
						break;
					case 1:
						val = vol.get_value(img_x, current_slice, img_y);
						break;
					case 2:
						val = vol.get_value(current_slice, img_x, img_y);
						break;
					}

					uint8_t intensity =
					    255 * (val - vol.win_min) / (vol.win_max - vol.win_min);
					fl_color(fl_rgb_color(intensity, intensity, intensity));
					fl_point(x() + ix, y() + iy);
				}
			}
		}

		// Draw zoom rectangle
		if (mode == Mode::ZOOM && is_dragging)
		{
			fl_color(FL_RED);
			fl_rect(drag_start_x, drag_start_y, Fl::event_x() - drag_start_x,
			        Fl::event_y() - drag_start_y);
		}
	}

	enum class Mode
	{
		NONE,
		ZOOM,
		DRAG
	};
	Mode mode;

private:
	Volume& vol;
	float zoom;
	int pan_x, pan_y;
	int drag_start_x, drag_start_y;
	bool is_dragging;
	int current_slice = 0;
	int orientation = 0;  // 0=XY, 1=XZ, 2=YZ
};

class ViewerWindow : public Fl_Window
{
public:
	ViewerWindow(int W, int H, Volume& vol)
	    : Fl_Window(W, H, "Volume Viewer"),
	      vol(vol),
	      canvas(200, 40, W - 220, H - 60, vol)
	{

		// Toolbar
		zoom_btn = new Fl_Button(10, 10, 80, 25, "Zoom");
		drag_btn = new Fl_Button(100, 10, 80, 25, "Drag");

		min_input = new Fl_Float_Input(200, 10, 80, 25, "Min:");
		max_input = new Fl_Float_Input(300, 10, 80, 25, "Max:");
		min_input->value(std::to_string(vol.min_val).c_str());
		max_input->value(std::to_string(vol.max_val).c_str());

		// Orientation radio buttons
		xy_radio = new Fl_Round_Button(400, 10, 80, 25, "XY");
		xz_radio = new Fl_Round_Button(480, 10, 80, 25, "XZ");
		yz_radio = new Fl_Round_Button(560, 10, 80, 25, "YZ");
		xy_radio->type(FL_RADIO_BUTTON);
		xz_radio->type(FL_RADIO_BUTTON);
		yz_radio->type(FL_RADIO_BUTTON);
		xy_radio->setonly();

		// Slice scrollbar
		scrollbar = new Fl_Scrollbar(10, H - 40, W - 20, 20);
		scrollbar->bounds(0, vol.nz - 1);
		scrollbar->value(0);

		// Callbacks
		zoom_btn->callback(zoom_cb, this);
		drag_btn->callback(drag_cb, this);
		min_input->callback(window_cb, this);
		max_input->callback(window_cb, this);
		xy_radio->callback(orientation_cb, this);
		xz_radio->callback(orientation_cb, this);
		yz_radio->callback(orientation_cb, this);
		scrollbar->callback(scroll_cb, this);
	}

private:
	Volume& vol;
	Canvas canvas;
	Fl_Button *zoom_btn, *drag_btn;
	Fl_Float_Input *min_input, *max_input;
	Fl_Round_Button *xy_radio, *xz_radio, *yz_radio;
	Fl_Scrollbar* scrollbar;

	static void zoom_cb(Fl_Widget*, void* v)
	{
		ViewerWindow* win = (ViewerWindow*)v;
		win->canvas.mode = Canvas::Mode::ZOOM;
	}

	static void drag_cb(Fl_Widget*, void* v)
	{
		ViewerWindow* win = (ViewerWindow*)v;
		win->canvas.mode = Canvas::Mode::DRAG;
	}

	static void window_cb(Fl_Widget* w, void* v)
	{
		ViewerWindow* win = (ViewerWindow*)v;
		float min = atof(win->min_input->value());
		float max = atof(win->max_input->value());
		win->canvas.set_window(min, max);
	}

	static void orientation_cb(Fl_Widget* w, void* v)
	{
		ViewerWindow* win = (ViewerWindow*)v;
		int orientation = 0;
		if (win->xz_radio->value())
			orientation = 1;
		if (win->yz_radio->value())
			orientation = 2;
		win->canvas.set_orientation(orientation);
	}

	static void scroll_cb(Fl_Widget* w, void* v)
	{
		ViewerWindow* win = (ViewerWindow*)v;
		win->canvas.update_slice(win->scrollbar->value());
	}
};

int main(int argc, char** argv)
{
	if (argc < 5)
	{
		printf("Usage: %s <file> <nx> <ny> <nz>\n", argv[0]);
		return 1;
	}

	Volume vol(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
	ViewerWindow win(800, 600, vol);
	win.show();
	return Fl::run();
}
