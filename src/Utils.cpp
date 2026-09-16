#include "../include/Utils.hpp"

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