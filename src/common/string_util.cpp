#include "string_util.h"

namespace pmkutil
{
	std::string StringReplace(std::string str, const std::string& from, const std::string& to)
	{
		size_t start_pos{ 0 };
		while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
			str.replace(start_pos, from.length(), to);
			start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
		}
		return str;
	}

	void LeftTrim(std::string& s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
			}));
	}

	void RightTrim(std::string& s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
			}).base(), s.end());
	}

	void Trim(std::string& s) {
		LeftTrim(s);
		RightTrim(s);
	}

	const char** StringArray::GetStringArray(uint32_t* count_out)
	{
		c_strings_.clear();

		for (const std::string& s : strings_) {
			c_strings_.push_back(s.c_str());
		}

		*count_out = (uint32_t)c_strings_.size();
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
}
