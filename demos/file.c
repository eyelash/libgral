#include <gral.h>
#include <stdio.h>
#include <string.h>

static void command_cat(int argc, char **argv) {
	if (argc < 3) {
		printf("missing argument\n");
		return;
	}
	struct gral_file *file = gral_file_open_read(argv[2]);
	if (!file) {
		printf("invalid file\n");
		return;
	}
	size_t size = gral_file_get_size(file);
	void *data = gral_file_map(file, size);
	gral_file_close(file);
	gral_file_write(gral_file_get_standard_output(), data, size);
	gral_file_unmap(data, size);
}

static void ls_callback(char const *name, void *user_data) {
	printf("%s\n", name);
}

static void command_ls(int argc, char **argv) {
	if (argc < 3) {
		printf("missing argument\n");
		return;
	}
	gral_directory_iterate(argv[2], &ls_callback, 0);
}

static void command_mkdir(int argc, char **argv) {
	if (argc < 3) {
		printf("missing argument\n");
		return;
	}
	gral_directory_create(argv[2]);
}

static void command_rm(int argc, char **argv) {
	if (argc < 3) {
		printf("missing argument\n");
		return;
	}
	gral_file_remove(argv[2]);
}

static void command_rmdir(int argc, char **argv) {
	if (argc < 3) {
		printf("missing argument\n");
		return;
	}
	gral_directory_remove(argv[2]);
}

static void command_stat(int argc, char **argv) {
	if (argc < 3) {
		printf("missing argument\n");
		return;
	}
	int file_type = gral_file_get_type(argv[2]);
	if (file_type == GRAL_FILE_TYPE_REGULAR) {
		printf("regular\n");
	}
	else if (file_type == GRAL_FILE_TYPE_DIRECTORY) {
		printf("directory\n");
	}
	else if (file_type == GRAL_FILE_TYPE_INVALID) {
		printf("invalid\n");
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("missing command\n");
		return 0;
	}
	if (strcmp(argv[1], "cat") == 0) {
		command_cat(argc, argv);
	}
	else if (strcmp(argv[1], "ls") == 0) {
		command_ls(argc, argv);
	}
	else if (strcmp(argv[1], "mkdir") == 0) {
		command_mkdir(argc, argv);
	}
	else if (strcmp(argv[1], "rm") == 0) {
		command_rm(argc, argv);
	}
	else if (strcmp(argv[1], "rmdir") == 0) {
		command_rmdir(argc, argv);
	}
	else if (strcmp(argv[1], "stat") == 0) {
		command_stat(argc, argv);
	}
	else {
		printf("invalid command\n");
	}
	return 0;
}
