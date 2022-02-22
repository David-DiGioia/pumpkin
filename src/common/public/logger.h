#pragma once

#include <cstdio>
#include <string>

#include "project_config.h"
#include "string_util.h"

namespace logger
{
	enum class TextColor {
		BLACK = 30,
		RED = 31,
		GREEN = 32,
		YELLOW = 33,
		BLUE = 34,
		MAGENTA = 35,
		CYAN = 36,
		WHITE = 37,
		BRIGHT_BLACK = 90,
		BRIGHT_RED = 91,
		BRIGHT_GREEN = 92,
		BRIGHT_YELLOW = 93,
		BRIGHT_BLUE = 94,
		BRIGHT_MAGENTA = 95,
		BRIGHT_CYAN = 96,
		BRIGHT_WHITE = 97,
	};

	std::string GetColoredText(const std::string& text, TextColor color);

	template<typename... Args>
	void Print(const std::string& f, Args... args) {
		if (!config::suppress_logger) {
			printf(f.c_str(), args...);
		}
	}

	template<typename... Args>
	void TaggedError(std::string tag, TextColor color, const std::string& f, Args... args) {

		tag = "[" + tag + "]: ";
		tag = GetColoredText(tag, color);
		printf("%s", tag.c_str());

		// Make enough whitespace to align text to right of tag.
		std::string whitespace(tag.size() + 4, ' ');
		whitespace = "\n" + whitespace;

		// Align text to be to the right of tag without leaving trailing whitespace.
		std::string f_indented{ pmkutil::StringReplace(f, "\n", whitespace) };
		pmkutil::RightTrim(f_indented);
		f_indented += "\n";

		std::string formatted{ GetColoredText(f_indented, TextColor::RED) };
		printf(formatted.c_str(), args...);
	}

	template<typename... Args>
	void Error(const std::string& f, Args... args) {
		TaggedError("Error", TextColor::RED, f, args...);
	}
}
