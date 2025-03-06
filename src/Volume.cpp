// Volume.cpp
#include "Volume.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

Volume::Volume(const std::string& filename, int nx, int ny, int nz)
    : nx_(nx), ny_(ny), nz_(nz)
{
	const size_t expected_size = nx * ny * nz * sizeof(float);
	std::ifstream file(filename, std::ios::binary);

	if (!file)
		throw std::runtime_error("Cannot open file: " + filename);

	file.seekg(0, std::ios::end);
	size_t actual_size = file.tellg();
	file.seekg(0, std::ios::beg);

	if (actual_size != expected_size)
		throw std::runtime_error("File size mismatch for " + filename);

	data_.resize(nx * ny * nz);
	file.read(reinterpret_cast<char*>(data_.data()), expected_size);

	auto [min_it, max_it] = std::minmax_element(data_.begin(), data_.end());
	global_min_ = *min_it;
	global_max_ = *max_it;
	reset_window();
}

float Volume::at(int x, int y, int z) const
{
	if (x < 0 || x >= nx_ || y < 0 || y >= ny_ || z < 0 || z >= nz_)
		return 0.0f;
	return data_[z * nx_ * ny_ + y * nx_ + x];
}

void Volume::set_window(float min, float max)
{
	window_min_ = min;
	window_max_ = max;
}
