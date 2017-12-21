#include <string>
#include <vector>

#include "Helper.hpp"

using std::string;
using std::vector;
using std::size_t;

int main(int argc, char **argv) {
	string input_path, output_path;
	get_real_filepath(argc, argv, input_path, output_path);

	build_contour_tree(input_path, output_path);
	return 0;
}





