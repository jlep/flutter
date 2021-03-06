#ifndef OPTIONS_H
#define OPTIONS_H

#include <memory>
#include <string>
#include <iosfwd>

namespace cv
{
class VideoCapture;
class VideoWriter;
}

namespace flutter
{

enum input_source {
        device_input,
        file_input
};

struct options {
	double ransac_good_ratio;
	double ransac_threshold;
	double process_error;
	double measurement_error;
	double low_pass;
	int avg_window;
	double fps;
	int delay;
	bool quiet;
	std::string codec;
	int fourcc;
	input_source input_src;
	std::string input_file;
	std::string output_file;
	std::string trajectory_file;
	int out_width;
	int out_height;
	int display_width;
	int display_height;
	bool show_original;
	double zoom;
	std::unique_ptr<cv::VideoCapture> capture;
	std::unique_ptr<cv::VideoWriter> writer;
	std::unique_ptr<std::ofstream> trajectory;

	options();
};

enum parse_status {
        fail,
        stop,
        cont
};

parse_status parse(options& opts, int argc, char* argv[]);

}

#endif // OPTIONS_H
