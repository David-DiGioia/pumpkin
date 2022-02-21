#pragma once

#include <cstdio>
#include <string>

#include "project_config.h"
#include "string_util.h"

namespace logger
{
	template<typename... Args>
	void Print(const std::string& f, Args... args) {
		if (!config::suppress_logger) {
			printf(f.c_str(), args...);
		}
	}

	template<typename... Args>
	void Error(const std::string& f, Args... args) {
		// These extra characters make the text red.
		printf("\x1B[31mError: \033[0m");

		// Align text to be to the right of "Error: " without leaving trailing whitespace.
		std::string f_indented{ pmkutil::StringReplace(f, "\n", "\n       ") };
		pmkutil::RightTrim(f_indented);
		f_indented += "\n";

		std::string formatted{ "\x1B[31m" + f_indented + "\033[0m"};
		printf(formatted.c_str(), args...);
	}

	template<typename... Args>
	void TaggedError(const std::string& tag, const std::string& f, Args... args) {
		// These extra characters make the text red.
		printf("\x1B[31m[%s]: \033[0m", tag.c_str());

		// Make enough whitespace to align text to right of tag.
		std::string whitespace(tag.size() + 4, ' ');
		whitespace = std::string{ "\n" } + whitespace;

		// Align text to be to the right of tag without leaving trailing whitespace.
		std::string f_indented{ pmkutil::StringReplace(f, "\n", whitespace) };
		pmkutil::RightTrim(f_indented);
		f_indented += "\n";

		std::string formatted{ "\x1B[31m" + f_indented + "\033[0m" };
		printf(formatted.c_str(), args...);
	}
}
