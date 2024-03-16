#ifndef LOGGER_H
#define LOGGER_H

#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>

#define ENOTINIT 200 // device not initialized

enum log_mode {
	SLOG_ONCE,
	SLOG_CIRC
};

class SensorLogger {
	/*
	 * Convenience class to log fixed-width sensor data
	 * One of the constructor's arguments is the logging mode
	 * For "once", write() returns -ENOSPC at the configured size and 
	 * For "circ", write() wraps around to the start and never ends
	 */

public:
	SensorLogger(char* fname, size_t sample_width, size_t n_samples, enum log_mode mode);

	int32_t init(void);
	int32_t open(void);
	int32_t close(void);
	int32_t write(uint8_t* src);
	int32_t read(uint8_t* dst, size_t idx);
	int32_t file_size();
	int32_t volume_free_space();
	
protected:
	int32_t stat(void);
	int32_t stat_vfs(void);

	const char* m_fname;
	const size_t m_width;
	const enum log_mode m_mode;

	const size_t m_size;
	bool m_initalized = false;
	struct fs_file_t m_file;
	struct fs_dirent m_dirent;
	struct fs_statvfs m_vfs;
	off_t m_wpos;
};

#endif
