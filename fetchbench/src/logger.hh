#pragma once

#include <cstdio>
#include <cstdarg>
#include <string>

using std::string;

class L {
private:
	static const int APPLICATION_LOG_LEVEL = 0;

	/**
	 * Logs a message to stdout if its log level is >= the application log
	 * level.
	 *
	 * @param[in]  level   The log level
	 * @param[in]  format  The format string
	 * @param[in]  args    The format string arguments
	 */
	static void log_message(int level, const char* format, va_list args){
		if (APPLICATION_LOG_LEVEL <= level) {
			vprintf(format, args);
			fflush(stdout);
		}
	}
public:
	/**
	 * Logs a message at debug log level.
	 *
	 * @param[in]  format     The format string
	 * @param[in]  <unnamed>  The format string parameters
	 */
	static void debug(const char* format, ...)
		__attribute__ ((format (printf, 1, 2)))
	{
		va_list args;
		va_start(args, format);
		log_message(0, format, args);
		va_end(args);
	}
	/**
	 * Logs a message at info log level.
	 *
	 * @param[in]  format     The format string
	 * @param[in]  <unnamed>  The format string parameters
	 */
	static void info(const char* format, ...)
		__attribute__ ((format (printf, 1, 2)))
	{
		va_list args;
		va_start(args, format);
		log_message(1, format, args);
		va_end(args);
	}
	/**
	 * Logs a message at warning log level.
	 *
	 * @param[in]  format     The format string
	 * @param[in]  <unnamed>  The format string parameters
	 */
	static void warn(const char* format, ...)
		__attribute__ ((format (printf, 1, 2)))
	{
		va_list args;
		va_start(args, format);
		log_message(2, format, args);
		va_end(args);
	}
	/**
	 * Logs a message at error log level.
	 *
	 * @param[in]  format     The format string
	 * @param[in]  <unnamed>  The format string parameters
	 */
	static void err(const char* format, ...)
		__attribute__ ((format (printf, 1, 2)))
	{
		va_list args;
		va_start(args, format);
		log_message(3, format, args);
		va_end(args);
	}
};
