#pragma once

#include <cstdio>
#include <cstdarg>
#include <string>

using std::string;

class L {
private:
	static const int APPLICATION_LOG_LEVEL = 0;

	static void log_message(int level, const char* format, va_list args){
		if (APPLICATION_LOG_LEVEL <= level) {
			vprintf(format, args);
			fflush(stdout);
		}
	}
public:
	static void debug(const char* format, ...)
		__attribute__ ((format (printf, 1, 2)))
	{
		va_list args;
		va_start(args, format);
		log_message(0, format, args);
		va_end(args);
	}
	static void info(const char* format, ...)
		__attribute__ ((format (printf, 1, 2)))
	{
		va_list args;
		va_start(args, format);
		log_message(1, format, args);
		va_end(args);
	}
	static void warn(const char* format, ...)
		__attribute__ ((format (printf, 1, 2)))
	{
		va_list args;
		va_start(args, format);
		log_message(2, format, args);
		va_end(args);
	}
	static void err(const char* format, ...)
		__attribute__ ((format (printf, 1, 2)))
	{
		va_list args;
		va_start(args, format);
		log_message(3, format, args);
		va_end(args);
	}
};
