#pragma once

#include <string>

class Utils {
private:
	//* Private constructors and destructor to prevent the class from being instantiated
	Utils();
	Utils(const Utils& other);
	Utils& operator=(const Utils& other);
	~Utils();

public:
	template <typename T>
	static std::string toString(const T& value);

	static bool isAllUpper(const std::string& str);

	static std::string getCurrentDateGMT();

	static std::string urlDecode(const std::string& str);
};
#include "Utils.tpp"