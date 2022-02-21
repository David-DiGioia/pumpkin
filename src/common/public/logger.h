#pragma once

#include <cstdio>
#include <string>

#include "project_config.h"

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
		std::string formatted{ "\x1B[31m" + f + "\033[0m"};
		printf(formatted.c_str(), args...);
	}
}
