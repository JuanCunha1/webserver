#include "../include/Utils.hpp"
#include <ctime>
#include <string>
#include <sstream>

bool Utils::isAllUpper(const std::string& str) {
	if (str.empty()) {
		return false;
	}
	for (std::size_t i = 0; i < str.length(); ++i) {
		if (!std::isupper(static_cast<unsigned char>(str[i]))) {
			return false;
		}
	}
	return true;
}

std::string Utils::getCurrentDateGMT() {
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[100];

    time(&rawtime);
    timeinfo = gmtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);

    return std::string(buffer);
}

//* Transforms characters like %2F (2F hexadecimal of the ascii table) into their character
std::string Utils::urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%') {
            // Make sure at least 2 characters remain after the '%'
            if (i + 2 < str.length()) {
                int value;
                std::istringstream is(str.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += '%'; // If the conversion fails, leave it as is
                }
            } else {
                result += '%';
            }
        } else if (str[i] == '+') {
            // '+' in a URL == space
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}