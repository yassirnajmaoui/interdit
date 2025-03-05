#include "Widgets.hpp"
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>

Button::Button(int x, int y, int width, int height, const std::string& label)
    : x_(x), y_(y), width_(width), height_(height), label_(label)
{
}

void Button::draw(Display* dpy, Window window, GC gc) const
{
	// Draw background
	unsigned long bg_color = WhitePixel(dpy, DefaultScreen(dpy));
	unsigned long fg_color = BlackPixel(dpy, DefaultScreen(dpy));

	if (state_ == ButtonState::PRESSED)
	{
		bg_color = BlackPixel(dpy, DefaultScreen(dpy));
		fg_color = WhitePixel(dpy, DefaultScreen(dpy));
	}
	else if (state_ == ButtonState::HOVER)
	{
		bg_color = 0xCCCCCC;
	}

	XSetForeground(dpy, gc, bg_color);
	XFillRectangle(dpy, window, gc, x_, y_, width_, height_);

	// Draw border
	XSetForeground(dpy, gc, fg_color);
	XDrawRectangle(dpy, window, gc, x_, y_, width_, height_);

	// Draw label
	XFontStruct* font = XLoadQueryFont(dpy, "fixed");
	if (font)
	{
		XSetFont(dpy, gc, font->fid);
		int label_width = XTextWidth(font, label_.c_str(), label_.length());
		int label_x = x_ + (width_ - label_width) / 2;
		int label_y = y_ + (height_ + font->ascent) / 2;

		XDrawString(dpy, window, gc, label_x, label_y, label_.c_str(),
		            label_.length());
		XFreeFont(dpy, font);
	}
}

bool Button::handle_event(const XEvent& event)
{
	const int ev_x = event.xbutton.x;
	const int ev_y = event.xbutton.y;
	bool inside = (ev_x >= x_ && ev_x <= x_ + width_ && ev_y >= y_ &&
	               ev_y <= y_ + height_);

	switch (event.type)
	{
	case ButtonPress:
		if (event.xbutton.button == Button1 && inside)
		{
			state_ = ButtonState::PRESSED;
			return true;
		}
		break;

	case ButtonRelease:
		if (event.xbutton.button == Button1)
		{
			if (state_ == ButtonState::PRESSED && inside)
			{
				if (is_toggle_)
				{
					toggle_state_ = !toggle_state_;
					state_ = toggle_state_ ? ButtonState::PRESSED :
					                         ButtonState::NORMAL;
				}
				else
				{
					state_ = ButtonState::NORMAL;
				}
				if (callback_)
					callback_();
				return true;
			}
			state_ = ButtonState::NORMAL;
		}
		break;

	case MotionNotify:
		state_ = inside ?
		             (state_ == ButtonState::PRESSED ? ButtonState::PRESSED :
		                                               ButtonState::HOVER) :
		             ButtonState::NORMAL;
		break;
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
	if (!is_toggle_)
	{
		state_ = ButtonState::NORMAL;
		toggle_state_ = false;
	}
}

void Button::set_pressed(bool p)
{
	if (p)
	{
		state_ = ButtonState::PRESSED;
	}
}

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
	// Draw background
	XSetForeground(dpy, gc, has_focus_ ? 0xDDDDFF : 0xFFFFFF);
	XFillRectangle(dpy, window, gc, x_, y_, width_, height_);

	// Draw border
	XSetForeground(dpy, gc, 0x000000);
	XDrawRectangle(dpy, window, gc, x_, y_, width_, height_);

	// Load font
	XFontStruct* font =
	    XLoadQueryFont(dpy, "-*-fixed-medium-*-*-*-14-*-*-*-*-*-*-*");
	if (!font)
		font = XLoadQueryFont(dpy, "fixed");

	// Draw text
	if (font)
	{
		XSetFont(dpy, gc, font->fid);
		const int text_y = y_ + height_ / 2 + font->ascent / 2;
		XDrawString(dpy, window, gc, x_ + 5, text_y, text_.c_str(),
		            text_.length());
		XFreeFont(dpy, font);
	}

	// Draw cursor if focused
	if (has_focus_)
	{
		update_cursor_blink();
		if (cursor_visible_)
		{
			XFontStruct* cursor_font = XLoadQueryFont(dpy, "fixed");
			const int cursor_x =
			    x_ + 5 +
			    (cursor_font ?
			         XTextWidth(cursor_font, text_.c_str(), text_.length()) :
			         0);
			XFreeFont(dpy, cursor_font);

			XDrawLine(dpy, window, gc, cursor_x, y_ + 4, cursor_x,
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
		const int ev_x = event.xbutton.x;
		const int ev_y = event.xbutton.y;
		bool clicked = (ev_x >= x_ && ev_x <= x_ + width_ && ev_y >= y_ &&
		                ev_y <= y_ + height_);

		if (clicked && !has_focus_)
		{
			has_focus_ = true;
			cursor_visible_ = true;
			last_blink_time_ = std::chrono::system_clock::now();
			return true;
		}
		else if (!clicked && has_focus_)
		{
			has_focus_ = false;
			return true;
		}
		break;
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

		// Handle special keys
		switch (keysym)
		{
		case XK_BackSpace:
			if (!text_.empty())
				text_.pop_back();
			break;

		case XK_Return: has_focus_ = false; break;

		default:
			if (count > 0 && isprint(buf[0]))
			{
				if (is_numeric_input())
				{
					// Allow numbers, decimal, and minus
					if (isdigit(buf[0]) || buf[0] == '.' || buf[0] == '-')
					{
						text_ += buf[0];
					}
				}
				else
				{
					text_ += buf[0];
				}
			}
			break;
		}
		cursor_visible_ = true;
		last_blink_time_ = std::chrono::system_clock::now();
		return true;
	}
	}
	return false;
}

void TextInput::update_cursor_blink() const
{
	auto now = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
	                   now - last_blink_time_)
	                   .count();

	if (elapsed > 500)
	{  // Blink every 500ms
		cursor_visible_ = !cursor_visible_;
		last_blink_time_ = now;
	}
}

bool TextInput::is_numeric_input() const
{
	return true;  // For our use case, always numeric
}

void TextInput::set_text(const std::string& text)
{
	text_ = text;
	if (is_numeric_input())
	{
		// Validate numeric input
		text_.erase(
		    std::remove_if(text_.begin(), text_.end(), [](char c)
		                   { return !isdigit(c) && c != '.' && c != '-'; }),
		    text_.end());
	}
}
