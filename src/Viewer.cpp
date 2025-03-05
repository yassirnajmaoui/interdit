#include "Viewer.hpp"
#include <X11/Xutil.h>
#include <cmath>
#include <stdexcept>
#include <unistd.h>

Viewer::Viewer(const std::vector<std::shared_ptr<Volume>>& volumes) {
    display_ = XOpenDisplay(nullptr);
    if(!display_) throw std::runtime_error("Cannot open X display");

    int screen = DefaultScreen(display_);
    window_ = XCreateSimpleWindow(display_, RootWindow(display_, screen),
                                 0, 0, 800, 600, 1,
                                 BlackPixel(display_, screen),
                                 WhitePixel(display_, screen));
    XStoreName(display_, window_, "Interdit - Volume Viewer");
    
    gc_ = XCreateGC(display_, window_, 0, nullptr);
    XSelectInput(display_, window_, ExposureMask | KeyPressMask | ButtonPressMask | 
                ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    
    // Initialize views
    for(auto& vol : volumes) {
        ViewState vs;
        vs.volume = vol;
        vs.min_input = std::make_unique<TextInput>(0, 0, 80, 25);
        vs.max_input = std::make_unique<TextInput>(90, 0, 80, 25);
        vs.zoom_btn = std::make_unique<Button>(180, 0, 60, 25, "Zoom");
        vs.drag_btn = std::make_unique<Button>(250, 0, 60, 25, "Drag");
        views_.push_back(std::move(vs));
    }

    XMapWindow(display_, window_);
}

Viewer::~Viewer() {
    XFreeGC(display_, gc_);
    XDestroyWindow(display_, window_);
    XCloseDisplay(display_);
}

void Viewer::run() {
    while(running_) {
        handle_events();
        update_colormap();
        draw_ui();
        XFlush(display_);
        usleep(10000);
    }
}

void Viewer::handle_events() {
    XEvent event;
    while(XPending(display_)) {
        XNextEvent(display_, &event);
        
        for(auto& view : views_) {
            if(view.min_input->handle_event(event) ||
               view.max_input->handle_event(event) ||
               view.zoom_btn->handle_event(event) ||
               view.drag_btn->handle_event(event)) return;
        }

        switch(event.type) {
            case Expose: draw_ui(); break;
            case ClientMessage:
                if(event.xclient.data.l[0] == XInternAtom(display_, "WM_DELETE_WINDOW", False))
                    running_ = false;
                break;
        }
    }
}

void Viewer::draw_ui() {
    XClearWindow(display_, window_);
    
    // Draw UI controls
    int y_offset = 40;
    for(auto& view : views_) {
        view.min_input->draw(display_, window_, gc_);
        view.max_input->draw(display_, window_, gc_);
        view.zoom_btn->draw(display_, window_, gc_);
        view.drag_btn->draw(display_, window_, gc_);
        draw_volume(view);
        y_offset += 300;
    }
}

void Viewer::draw_volume(const ViewState& view) {
    // Simplified rendering logic
    const int w = view.volume->nx();
    const int h = view.volume->ny();
    
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            float val = view.volume->at(x, y, 0);
            uint8_t intensity = static_cast<uint8_t>(255 * (val - view.volume->window_min()) /
                                                     (view.volume->window_max() - view.volume->window_min()));
            XSetForeground(display_, gc_, intensity << 16 | intensity << 8 | intensity);
            XDrawPoint(display_, window_, gc_, x + view.pan_x, y + view.pan_y);
        }
    }
}

void Viewer::update_colormap() {
    for(auto& view : views_) {
        try {
            float min = std::stof(view.min_input->get_text());
            float max = std::stof(view.max_input->get_text());
            view.volume->set_window(min, max);
        } catch(...) {
            // Invalid input, retain previous values
        }
    }
}
