#pragma once

#include <vector>
#include <string>

class StringArray
{
public:
	// Returned array should be used in same scope that this function is called.
	// Also the StringArray that this is called from must not be destroyed while
	// the returned array is in use.
	const char** GetStringArray(uint32_t* count_out);

	void PushBack(const std::string& s);

	void PushBack(const char** arr, int length);

private:
	std::vector<std::string> strings_{};
	std::vector<const char*> c_strings_{};
};
