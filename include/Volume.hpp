// Volume.hpp
#pragma once
#include <vector>
#include <string>
#include <memory>

class Volume {
public:
    Volume(const std::string& filename, int nx, int ny, int nz);
    
    // Accessors
    int nx() const { return nx_; }
    int ny() const { return ny_; }
    int nz() const { return nz_; }
    float min() const { return global_min_; }
    float max() const { return global_max_; }
    float window_min() const { return window_min_; }
    float window_max() const { return window_max_; }
    
    // Data access
    float at(int x, int y, int z) const;
    const float* data() const { return data_.data(); }
    
    // Window management
    void set_window(float min, float max);
    void reset_window() { set_window(global_min_, global_max_); }

private:
    std::vector<float> data_;
    int nx_, ny_, nz_;
    float global_min_, global_max_;
    float window_min_, window_max_;
};
