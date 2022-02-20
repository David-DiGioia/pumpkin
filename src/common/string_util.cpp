#include "string_util.h"

const char** StringArray::GetStringArray(uint32_t* count_out)
{
	c_strings_.clear();
	
	for (const std::string& s : strings_) {
		c_strings_.push_back(s.c_str());
	}

	*count_out = c_strings_.size();
	return c_strings_.data();
}

void StringArray::PushBack(const std::string& s)
{
	strings_.push_back(s);
}

void StringArray::PushBack(const char* const* arr, uint32_t length)
{
	for (uint32_t i{ 0 }; i < length; ++i) {
		strings_.push_back(arr[i]);
	}
}

std::vector<std::string>::const_iterator StringArray::begin() const
{
	return strings_.cbegin();
}

std::vector<std::string>::const_iterator StringArray::end() const
{
	return strings_.cend();
}
