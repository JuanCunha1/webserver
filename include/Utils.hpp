#pragma once

#include <string>

class Utils {
private:
	// Constructores y destructor privados para evitar que la clase se instancie
	Utils();
	Utils(const Utils& other);
	Utils& operator=(const Utils& other);
	~Utils();

public:
	template <typename T>
	static std::string toString(const T& value);
};

#include "Utils.tpp"