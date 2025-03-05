#include "CLI11.hpp"
#include "Viewer.hpp"
#include "Volume.hpp"
#include <memory>
#include <vector>

int main(int argc, char* argv[])
{
	CLI::App app{"3D Volume Viewer"};

	std::vector<std::string> filenames;
	std::vector<int> nx, ny, nz;
	bool sync_colormap = false;

	app.add_option("--image", filenames)
	    ->required()
	    ->check(CLI::ExistingFile)
	    ->description("Volume data file(s)");

	app.add_option("--nx", nx)->required()->description("X dimension(s)");

	app.add_option("--ny", ny)->required()->description("Y dimension(s)");

	app.add_option("--nz", nz)->required()->description("Z dimension(s)");

	app.add_flag("--sync", sync_colormap)
	    ->description("Synchronize color maps");

	CLI11_PARSE(app, argc, argv);

	// Validate equal number of parameters
	if (filenames.size() != nx.size() || filenames.size() != ny.size() ||
	    filenames.size() != nz.size())
	{
		std::cerr
		    << "Number of images must match number of dimension parameters\n";
		return 1;
	}

	// Load volumes
	std::vector<std::shared_ptr<Volume>> volumes;
	try
	{
		for (size_t i = 0; i < filenames.size(); i++)
		{
			volumes.emplace_back(
			    std::make_shared<Volume>(filenames[i], nx[i], ny[i], nz[i]));
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error loading volumes: " << e.what() << "\n";
		return 1;
	}

	// Launch viewer
	try
	{
		Viewer viewer(volumes);
		viewer.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Viewer error: " << e.what() << "\n";
		return 1;
	}

	return 0;
}
