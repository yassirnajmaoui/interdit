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
	void set_pressed(bool is_pressed);

	int x_, y_, width_, height_;
private:
	enum class ButtonState { NORMAL, PRESSED, HOVER };

	std::string label_;
	ButtonState state_ = ButtonState::NORMAL;
	std::function<void()> callback_;
	bool is_toggle_ = false;
	bool toggle_state_ = false;
};

// Add new widget classes
class Scrollbar {
public:
	Scrollbar(int x, int y, int height);
	void draw(Display* dpy, Window window, GC gc) const;
	bool handle_event(const XEvent& event);
	void set_range(int min, int max);
	int get_value() const { return current_value_; }

	int x_, y_, height_;
private:
	int min_value_ = 0;
	int max_value_ = 100;
	int current_value_ = 0;
	bool dragging_ = false;
};

class RadioButton {
public:
	RadioButton(int x, int y, const std::string& label);
	void draw(Display* dpy, Window window, GC gc) const;
	bool handle_event(const XEvent& event);
	void set_selected(bool selected);
	bool is_selected() const { return selected_; }

	int x_, y_;
private:
	std::string label_;
	bool selected_ = false;
};
