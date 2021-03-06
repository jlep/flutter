#include "options.h"
#include "opt_parser.h"
#include "suppress.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <locale>
#include <iterator>
#include <functional>
#include <cstdlib>

using namespace std;

struct stop_exception {};
struct fail_exception {};

static int get_fourcc(std::string const& codec)
{
	assert(codec.size() == 4);
	return CV_FOURCC(codec[0], codec[1], codec[2], codec[3]);
}

static std::string to_upper(std::string const& str)
{
	using namespace placeholders;
	string up;
	transform(str.begin(), str.end(), back_inserter(up),
		bind(toupper<char>, _1, cref(locale::classic())));
	return up;
}

static std::string get_codec(int fourcc)
{
	const locale& loc = locale::classic();
	string codec(4, ' ');
	codec[0] = toupper<char>(fourcc & 255, loc);
	codec[1] = toupper<char>((fourcc >> 8) & 255, loc);
	codec[2] = toupper<char>((fourcc >> 16) & 255, loc);
	codec[3] = toupper<char>((fourcc >> 24) & 255, loc);
	return codec;
}

flutter::options::options():
	ransac_good_ratio(0.5),
	ransac_threshold(0.05),
	process_error(0.5),
	measurement_error(0.5),
	low_pass(0.1),
	avg_window(0),
	fps(30.0),
	zoom(0.0),
	show_original(false),
	quiet(false),
	codec("MJPG"),
	fourcc(get_fourcc(codec)),
	input_src(device_input)
{
}

static const flutter::options default_opts;

static void print_help()
{
	cout <<
		"flutter - real time video stabilizer\n"
		"usage: flutter [options] [infile]\n"
		"\n"
		"  -h, --help                       Display help and exit.\n"
		"  -r, --ransac-ratio=<float>       Minimum ratio of points for an acceptable\n"
		"                                   model. The default is " << default_opts.ransac_good_ratio << ".\n"
		"  -n, --ransac-threshold=<float>   Maximum inlier distance relative to image\n"
		"                                   dimensions. The default is " << default_opts.ransac_threshold << ".\n"
		"  -p, --process-noise=<float>      Kalman process noise relative to image"
		"                                   dimensions. The default is " << default_opts.process_error << "\n"
		"  -m, --measurement-noise=<float>  Kalman measurement noise relative to image"
		"                                   dimensions. The default is " << default_opts.measurement_error << "\n"
		"  -l, --low-pass=<float>           Low pass filter magnitude. The default is " << default_opts.low_pass << ".\n"
		"  -a, --avg-window=<int>           Centered moving average window size. Overrides\n"
		"                                   default exponential low-pass filter if set.\n"
		"  -d, --device=<int>               Input device number. The default is 0.\n"
		"                                   If infile is given, it overrides this setting.\n"
		"  -f, --fps=<float>                Frames per second. Only relevant when output is shown.\n"
		"                                   The default is " << default_opts.fps << ".\n"
		"                                   If the input is a file, the output file will\n"
		"                                   have the same fps as the input file\n"
		"                                   regardless of this setting.\n"
		"  -x, --show-original              Show both original and stabilized video on top\n"
		"                                   of each other.\n"
		"  -q, --quiet                      Do not display output video.\n"
		"  -c, --codec=<fourcc>             Output codec as a four-character code (fourcc).\n"
		"                                   By default the codec is the same as with the\n"
		"                                   infile, or '" << default_opts.codec << "'),\n"
		"                                   if the source is a device. The available\n"
		"                                   codecs can be found at\n"
		"                                   http://www.fourcc.org/codecs.php\n"
		"  -o, --output=<file>              Output file.\n"
		"  -s, --size=<size>                Output frame size. Given as a single scale\n"
		"                                   number <scale> or as <width>x<height>,\n"
		"                                   for example, 640x480. Either of the values\n"
		"                                   may be empty, in which case the value is\n"
		"                                   calculated by preserving the original aspect\n"
		"                                   ratio. By default the original size is used.\n"
		"  -z, --zoom=<float>               Scale the video by the given factor.\n"
		"  -t, --trajectory=<file>          Trajectory data output file.\n"
		;
}

template <typename T = int>
static bool get_video_capture(unique_ptr<cv::VideoCapture>& vc, const T& src = 0)
{
	std_to_null stn;
	unique_ptr<cv::VideoCapture> tmp = make_unique<cv::VideoCapture>(src);
	if (!tmp->isOpened())
		return false;
	vc.swap(tmp);
	return true;
}

flutter::parse_status flutter::parse(options& opts, int argc, char* argv[])
{
	opt::parser op;
	double scale = 0.0;
	int out_width = 0;
	int out_height = 0;
	bool fourcc_set = false;
	op.add('r', "ransac-ratio", &opts.ransac_good_ratio);
	op.add('n', "ransac-threshold", &opts.ransac_threshold);
	op.add('p', "process-noise", &opts.process_error);
	op.add('m', "measurement-noise", &opts.measurement_error);
	op.add('l', "low-pass", &opts.low_pass);
	op.add('a', "avg-window", &opts.avg_window);
	op.add('f', "fps", &opts.fps);
	op.add('x', "show-original", &opts.show_original);
	op.add('q', "quiet", &opts.quiet);
	op.add('t', "trajectory", &opts.trajectory_file);
	op.add('z', "zoom", &opts.zoom);
	op.add('c', "codec", [&](const std::string& code) {
		if (code.size() != 4) {
			cerr << "fourcc should be exactly 4 characters long" <<
				endl;
			throw fail_exception();
		}
		opts.codec = to_upper(code);
		opts.fourcc = get_fourcc(opts.codec);
		fourcc_set = true;
	});
	op.add('d', "device", [&](int device) {
		if (!get_video_capture(opts.capture, device)) {
			cerr << "unable to open device " << device << endl;
			throw fail_exception();
		}
		opts.input_src = device_input;
	});
	op.add('o', "output", &opts.output_file);
	op.add('s', "size", [&](const std::string& size) {
		scale = 0.0;
		out_width = 0;
		out_height = 0;
		size_t x = size.find('x');
		if (x == string::npos) {
			size_t pos;
			try {
				scale = stof(size, &pos);
			} catch (logic_error const& err) {
				throw opt::parse_error();
			}
			if (pos != size.size())
				throw opt::parse_error();
			return;
		}
		char const* str = size.c_str();
		char* endptr;
		if (x > 0) {
			out_width = strtol(str, &endptr, 10);
			if (!out_width) {
				throw opt::parse_error();
			}
		}
		if (x < size.size()-1) {
			out_height = strtol(str+x+1, &endptr, 10);
			if (!out_height) {
				throw opt::parse_error();
			}
		}
	});
	op.add('h', "help", []() {
		print_help();
		throw stop_exception();
	});
	try {
		op.parse(argc, argv);
	} catch (const opt::parse_error& err) {
		cerr << "parse error: " << err.what() << endl;
		return fail;
	} catch (const opt::no_argument_error& err) {
		cerr << "no argument expected: " << err.what() << endl;
		return fail;
	} catch (const opt::required_argument_error& err) {
		cerr << "argument expected: " << err.what() << endl;
		return fail;
	} catch (const opt::unknown_option_error& err) {
		cerr << "unknown option: " << err.what() << endl;
		return fail;
	} catch (const stop_exception& err) {
		return stop;
	} catch (const fail_exception& err) {
		return fail;
	}
	if (op.pos_args.size() > 1) {
		cerr << "at most one infile expected" << endl;
		return fail;
	} else if (op.pos_args.size() == 1) {
		opts.input_file = op.pos_args.front();
		if (!get_video_capture(opts.capture, opts.input_file)) {
			cerr << "unable to open file " << opts.input_file << endl;
			return fail;
		}
		opts.input_src = file_input;
	}
	if (!opts.capture && !get_video_capture(opts.capture)) {
		cerr << "unable to open default device" << endl;
		return fail;
	}
	int in_width = opts.capture->get(CV_CAP_PROP_FRAME_WIDTH);
	int in_height = opts.capture->get(CV_CAP_PROP_FRAME_HEIGHT);
	if (scale > 0) {
		opts.out_width = scale * in_width;
		opts.out_height = scale * in_height;
	} else if (out_width > 0 && out_height > 0) {
		opts.out_width = out_width;
		opts.out_height = out_height;
	} else if (out_width > 0) {
		opts.out_width = out_width;
		opts.out_height = out_width * in_height / in_width;
	} else if (out_height > 0) {
		opts.out_height = out_height;
		opts.out_width = out_height * in_width / in_height;
	} else {
		opts.out_width = in_width;
		opts.out_height = in_height;
	}
	opts.display_width = opts.out_width;
	opts.display_height = opts.out_height;
	if (opts.show_original) {
		if (opts.out_width > opts.out_height)
			opts.display_height *= 2;
		else
			opts.display_width *= 2;
	}
	if (opts.input_file.empty()) {
		opts.capture->set(CV_CAP_PROP_FPS, opts.fps);
	}
	if (!opts.input_file.empty()) {
		opts.fps = opts.capture->get(CV_CAP_PROP_FPS);
		if (!fourcc_set) {
			opts.fourcc = opts.capture->get(CV_CAP_PROP_FOURCC);
			opts.codec = get_codec(opts.fourcc);
		}
	}
	if (!opts.output_file.empty()) {

		cv::Size size(opts.display_width, opts.display_height);
		opts.writer = make_unique<cv::VideoWriter>(opts.output_file,
			opts.fourcc, opts.fps, size);
		if (!opts.writer->isOpened()) {
			cerr << "unable to open file " << opts.output_file << endl;
			return fail;
		}
	}
	if (!opts.trajectory_file.empty()) {
		opts.trajectory = make_unique<ofstream>(opts.trajectory_file);
		if (opts.trajectory->fail()) {
			cerr << "unable to open file " << opts.trajectory_file << endl;
			return fail;
		}
	}
	opts.delay = 1000/opts.fps;
	return cont;
}

