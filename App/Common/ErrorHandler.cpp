#include "ErrorHandler.hpp"

#include <stdexcept>
#include <sstream>

using namespace LearnVulkan;

void ErrorHandler::throwError(const char* const prefix_info, const std::source_location& src) {
	std::ostringstream msg;
	msg << prefix_info << '\n';
	msg << src.file_name() << ':' << src.function_name() << ":(" << src.line() << ')' << std::endl;
	throw std::runtime_error(msg.str());
}