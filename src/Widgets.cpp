#include "Widgets.hpp"
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <iostream>

// TextInput implementation
TextInput::TextInput(int x, int y, int width, int height)
    : x_(x),
      y_(y),
      width_(width),
      height_(height),
      last_blink_time_(std::chrono::system_clock::now())
{
}

void TextInput::draw(Display* dpy, Window window, GC gc) const
{
	XSetForeground(dpy, gc, has_focus_ ? 0xDDDDFF : 0xFFFFFF);
	XFillRectangle(dpy, window, gc, x_, y_, width_, height_);

	XSetForeground(dpy, gc, 0x000000);
	XDrawRectangle(dpy, window, gc, x_, y_, width_, height_);

	XFontStruct* font = XLoadQueryFont(dpy, "fixed");
	if (font)
	{
		XSetFont(dpy, gc, font->fid);
		const int text_y = y_ + height_ / 2 + font->ascent / 2;
		XDrawString(dpy, window, gc, x_ + 5, text_y, text_.c_str(),
		            text_.length());
		XFreeFont(dpy, font);
	}

	if (has_focus_)
	{
		update_cursor_blink();
		if (cursor_visible_)
		{
			XDrawLine(dpy, window, gc,
			          x_ + 5 +
			              XTextWidth(XLoadQueryFont(dpy, "fixed"),
			                         text_.c_str(), text_.length()),
			          y_ + 4,
			          x_ + 5 +
			              XTextWidth(XLoadQueryFont(dpy, "fixed"),
			                         text_.c_str(), text_.length()),
			          y_ + height_ - 4);
		}
	}
}

bool TextInput::handle_event(const XEvent& event)
{
	switch (event.type)
	{
	case ButtonPress:
	{
		bool clicked =
		    (event.xbutton.x >= x_ && event.xbutton.x <= x_ + width_ &&
		     event.xbutton.y >= y_ && event.xbutton.y <= y_ + height_);
		if (clicked)
			has_focus_ = true;
		else
			has_focus_ = false;
		return clicked;
	}
	case KeyPress:
	{
		if (!has_focus_)
			return false;

		char buf[32];
		KeySym keysym;
		XComposeStatus compose;
		int count = XLookupString(const_cast<XKeyEvent*>(&event.xkey), buf,
		                          sizeof(buf), &keysym, &compose);

		switch (keysym)
		{
		case XK_BackSpace:
			if (!text_.empty())
				text_.pop_back();
			break;
		case XK_Return: has_focus_ = false; break;
		default:
			if (count > 0 &&
			    (isdigit(buf[0]) || buf[0] == '.' || buf[0] == '-'))
				text_ += buf[0];
			break;
		}
		return true;
	}
	}
	return false;
}

void TextInput::update_cursor_blink() const
{
	auto now = std::chrono::system_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now -
	                                                          last_blink_time_)
	        .count() > 500)
	{
		cursor_visible_ = !cursor_visible_;
		last_blink_time_ = now;
	}
}

// Button implementation
Button::Button(int x, int y, int width, int height, const std::string& label)
    : x_(x), y_(y), width_(width), height_(height), label_(label)
{
}

void Button::draw(Display* dpy, Window window, GC gc) const
{
	unsigned long bg = WhitePixel(dpy, DefaultScreen(dpy));
	unsigned long fg = BlackPixel(dpy, DefaultScreen(dpy));

	if (state_ == ButtonState::PRESSED)
		std::swap(bg, fg);
	else if (state_ == ButtonState::HOVER)
		bg = 0xCCCCCC;

	XSetForeground(dpy, gc, bg);
	XFillRectangle(dpy, window, gc, x_, y_, width_, height_);
	XSetForeground(dpy, gc, fg);
	XDrawRectangle(dpy, window, gc, x_, y_, width_, height_);

	XFontStruct* font = XLoadQueryFont(dpy, "fixed");
	if (font)
	{
		XSetFont(dpy, gc, font->fid);
		int lw = XTextWidth(font, label_.c_str(), label_.length());
		XDrawString(dpy, window, gc, x_ + (width_ - lw) / 2,
		            y_ + height_ / 2 + 5, label_.c_str(), label_.length());
		XFreeFont(dpy, font);
	}
}

bool Button::handle_event(const XEvent& event)
{
	const bool inside =
	    (event.xbutton.x >= x_ && event.xbutton.x <= x_ + width_ &&
	     event.xbutton.y >= y_ && event.xbutton.y <= y_ + height_);

	switch (event.type)
	{
	case ButtonPress:
		if (inside)
			state_ = ButtonState::PRESSED;
		return inside;
	case ButtonRelease:
		if (state_ == ButtonState::PRESSED && inside && callback_)
			callback_();
		state_ = ButtonState::NORMAL;
		return inside;
	case MotionNotify:
		state_ = inside ? ButtonState::HOVER : ButtonState::NORMAL;
		return inside;
	}
	return false;
}

void Button::set_callback(std::function<void()> callback)
{
	callback_ = callback;
}

void Button::set_toggle(bool is_toggle)
{
	is_toggle_ = is_toggle;
}

// Scrollbar implementation
Scrollbar::Scrollbar(int x, int y, int height) : x_(x), y_(y), height_(height)
{
}

void Scrollbar::draw(Display* dpy, Window window, GC gc) const
{
	// Draw track
	XSetForeground(dpy, gc, 0x888888);
	XFillRectangle(dpy, window, gc, x_, y_, 15, height_);

	// Calculate thumb position
	float ratio = static_cast<float>(current_value_ - min_value_) /
	              (max_value_ - min_value_);
	int thumb_y = y_ + ratio * (height_ - 20);

	// Draw thumb
	XSetForeground(dpy, gc, 0x444444);
	XFillRectangle(dpy, window, gc, x_, thumb_y, 15, 20);
}

bool Scrollbar::handle_event(const XEvent& event)
{
	const int scrollbar_width = 15;

	// Check if within horizontal bounds
	bool in_x =
	    (event.xbutton.x >= x_) && (event.xbutton.x <= x_ + scrollbar_width);

	if (event.type == ButtonPress && event.xbutton.button == Button1 && in_x)
	{
		int click_y = event.xbutton.y - y_;
		float ratio = static_cast<float>(click_y) / height_;
		current_value_ = min_value_ + ratio * (max_value_ - min_value_);
		current_value_ = std::clamp(current_value_, min_value_, max_value_);
		dragging_ = true;
		return true;
	}

	if (event.type == MotionNotify && dragging_ && in_x)
	{
		int new_y = event.xmotion.y - y_;
		float ratio =
		    std::max(0.0f, std::min(1.0f, static_cast<float>(new_y) / height_));
		current_value_ = min_value_ + ratio * (max_value_ - min_value_);
		return true;
	}

	if (event.type == ButtonRelease && event.xbutton.button == Button1)
	{
		dragging_ = false;
		return in_x;  // Only return true if released in scrollbar area
	}

	return false;
}

void Scrollbar::set_range(int min, int max)
{
	min_value_ = min;
	max_value_ = max;
	current_value_ = std::clamp(current_value_, min, max);
}

// RadioButton implementation
RadioButton::RadioButton(int x, int y, const std::string& label)
    : x_(x), y_(y), label_(label)
{
}

void RadioButton::draw(Display* dpy, Window window, GC gc) const
{
	// Draw outer circle
	XSetForeground(dpy, gc, 0x000000);
	XDrawArc(dpy, window, gc, x_, y_, 16, 16, 0, 360 * 64);

	// Draw selection indicator
	if (selected_)
	{
		XSetForeground(dpy, gc, 0x0000FF);
		XFillArc(dpy, window, gc, x_ + 4, y_ + 4, 8, 8, 0, 360 * 64);
	}

	// Draw label
	XFontStruct* font = XLoadQueryFont(dpy, "fixed");
	if (font)
	{
		XSetFont(dpy, gc, font->fid);
		XDrawString(dpy, window, gc, x_ + 20, y_ + 12, label_.c_str(),
		            label_.length());
		XFreeFont(dpy, font);
	}
}

bool RadioButton::handle_event(const XEvent& event)
{
	if (event.type == ButtonPress && event.xbutton.button == Button1)
	{
		int dx = event.xbutton.x - x_;
		int dy = event.xbutton.y - y_;
		if (dx >= 0 && dx <= 30 && dy >= 0 && dy <= 16)
		{
			selected_ = true;
			return true;
		}
	}
	return false;
}

void RadioButton::set_selected(bool selected)
{
	selected_ = selected;
}
