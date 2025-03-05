#pragma once
#include <X11/Xlib.h>
#include <string>
#include <functional>
#include <chrono>
#include <memory>

class TextInput {
public:
	TextInput(int x, int y, int width, int height);
	void draw(Display* dpy, Window window, GC gc) const;
	bool handle_event(const XEvent& event);
	void set_text(const std::string& text);
	std::string get_text() const { return text_; }
	bool has_focus() const { return has_focus_; }

	int x_, y_, width_, height_;
private:
	void update_cursor_blink() const;
	bool is_numeric_input() const;

	std::string text_;
	bool has_focus_ = false;
	mutable bool cursor_visible_ = false;
	mutable std::chrono::time_point<std::chrono::system_clock> last_blink_time_;
};

class Button {
public:
	Button(int x, int y, int width, int height, const std::string& label);
	void draw(Display* dpy, Window window, GC gc) const;
	bool handle_event(const XEvent& event);
	void set_callback(std::function<void()> callback);
	void set_toggle(bool is_toggle);
	bool is_pressed() const { return state_ == ButtonState::PRESSED; }

	int x_, y_, width_, height_;
private:
	enum class ButtonState { NORMAL, PRESSED, HOVER };

	std::string label_;
	ButtonState state_ = ButtonState::NORMAL;
	std::function<void()> callback_;
	bool is_toggle_ = false;
	bool toggle_state_ = false;
};
