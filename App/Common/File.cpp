#include "File.hpp"

#include <fstream>
#include <iterator>

#include <stdexcept>

using std::string, std::ifstream;
using std::ios;
using std::runtime_error;

using namespace LearnVulkan;

string File::readString(const char* const filename) {
	ifstream input(filename);
	if (!input) {
		using namespace std::string_literals;
		throw runtime_error("Unable to open file\'"s + filename + "\'"s);
	}

	string content;
	//find the length of this file so we can preallocate memory
	input.seekg(0, ios::end);
	content.reserve(input.tellg());
	input.seekg(0, ios::beg);

	using stream_it = std::istreambuf_iterator<string::value_type>;
	content.assign(stream_it(input), stream_it());
	return content;
}