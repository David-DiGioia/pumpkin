#include "logger.h"

std::string logger::GetColoredText(const std::string& text, TextColor color)
{
	return "\x1B[" + std::to_string((int)color) + "m" + text + "\033[0m";
}
