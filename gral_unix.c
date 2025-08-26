/*

Copyright (c) 2016-2025 Elias Aebi

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "gral.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>


/*=========
    FILE
 =========*/

struct gral_file *gral_file_open_read(char const *path) {
	int fd = open(path, O_RDONLY);
	return fd == -1 ? NULL : (struct gral_file *)(intptr_t)fd;
}

struct gral_file *gral_file_open_write(char const *path) {
	int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
	return fd == -1 ? NULL : (struct gral_file *)(intptr_t)fd;
}

struct gral_file *gral_file_get_standard_input(void) {
	return (struct gral_file *)STDIN_FILENO;
}

struct gral_file *gral_file_get_standard_output(void) {
	return (struct gral_file *)STDOUT_FILENO;
}

struct gral_file *gral_file_get_standard_error(void) {
	return (struct gral_file *)STDERR_FILENO;
}

void gral_file_close(struct gral_file *file) {
	close((int)(intptr_t)file);
}

size_t gral_file_read(struct gral_file *file, void *buffer, size_t size) {
	return read((int)(intptr_t)file, buffer, size);
}

void gral_file_write(struct gral_file *file, void const *buffer, size_t size) {
	write((int)(intptr_t)file, buffer, size);
}

size_t gral_file_get_size(struct gral_file *file) {
	struct stat s;
	fstat((int)(intptr_t)file, &s);
	return s.st_size;
}

void *gral_file_map(struct gral_file *file, size_t size) {
	void *address = mmap(NULL, size, PROT_READ, MAP_PRIVATE, (int)(intptr_t)file, 0);
	return address == MAP_FAILED ? NULL : address;
}

void gral_file_unmap(void *address, size_t size) {
	munmap(address, size);
}

int gral_file_get_type(char const *path) {
	struct stat s;
	if (stat(path, &s) == 0) {
		if (S_ISREG(s.st_mode)) return GRAL_FILE_TYPE_REGULAR;
		if (S_ISDIR(s.st_mode)) return GRAL_FILE_TYPE_DIRECTORY;
	}
	return GRAL_FILE_TYPE_INVALID;
}

void gral_file_rename(char const *old_path, char const *new_path) {
	rename(old_path, new_path);
}

void gral_file_remove(char const *path) {
	unlink(path);
}

void gral_directory_create(char const *path) {
	mkdir(path, 0777);
}

void gral_directory_iterate(char const *path, void (*callback)(char const *name, void *user_data), void *user_data) {
	DIR *directory = opendir(path);
	struct dirent *entry;
	while (entry = readdir(directory)) {
		callback(entry->d_name, user_data);
	}
	closedir(directory);
}

void gral_directory_remove(char const *path) {
	rmdir(path);
}

char *gral_get_current_working_directory(void) {
	return getcwd(NULL, 0);
}


/*=========
    TIME
 =========*/

double gral_time_get_monotonic(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

void gral_sleep(double seconds) {
	struct timespec ts;
	ts.tv_sec = seconds;
	ts.tv_nsec = (seconds - ts.tv_sec) * 1e9;
	nanosleep(&ts, NULL);
}
