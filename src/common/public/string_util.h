#pragma once

#include <vector>
#include <string>

namespace pmkutil
{
	std::string StringReplace(std::string str, const std::string& from, const std::string& to);

	// Acts as a wrapper around a const char**, allowing you to return a StringArray from a function
	// without dangling pointers / ownership problems.
	class StringArray
	{
	public:
		// Returned array should be used in same scope that this function is called.
		// Also the StringArray that this is called from must not be destroyed while
		// the returned array is in use.
		const char** GetStringArray(uint32_t* count_out);

		void PushBack(const std::string& s);

		void PushBack(const char* const* arr, uint32_t length);

		std::vector<std::string>::const_iterator begin() const;

		std::vector<std::string>::const_iterator end() const;

	private:
		std::vector<std::string> strings_{};
		std::vector<const char*> c_strings_{};
	};
}
