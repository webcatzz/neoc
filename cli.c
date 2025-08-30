#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <termios.h>
#include "cli.h"
#include "api.h"
#include "json.h"

#define KEY_SIZE (KEY_LENGTH + 1)
#define ARRAY_REALLOC_STEP 32
#define ERROR_FILE_LIST_EMPTY "couldn't find any files"
#define ERROR_RESPONSE_FETCH "couldn't fetch response"
#define ERROR_RESPONSE_PARSE "couldn't parse response"

int main(int argc, const char** args) {
	srand(time(NULL));
	argc--, args++;
	if (!argc) cmd_help(0, NULL);
	else {
		void(*command)(size_t, const char**) = NULL;
		if (!strcmp(*args, "help")) command = cmd_help;
		else if (!strcmp(*args, "info")) command = cmd_info;
		else if (!strcmp(*args, "list")) command = cmd_list;
		else if (!strcmp(*args, "upload")) command = cmd_upload;
		else if (!strcmp(*args, "delete")) command = cmd_delete;
		else if (!strcmp(*args, "diff")) command = cmd_diff;
		else {print_error("unrecognized command: %s", *args); return 1;}
		command(argc - 1, args + 1);
	}
	return 0;
}

/* helpers */

// grabs key from environment if available, or asks for user input
void get_key(char key[KEY_SIZE]) {
	char* env = getenv("NEOCAPI");
	if (env) {memcpy(key, env, KEY_SIZE); return;}

  struct termios old_flags, new_flags;
  if (tcgetattr(fileno(stdin), &old_flags)) return;
  new_flags = old_flags;
	new_flags.c_lflag &= ~ECHO;
  if (tcsetattr(fileno(stdin), TCSAFLUSH, &new_flags)) return;
	print_input("(need your api key...)");
	fgets(key, KEY_SIZE, stdin);
	tcsetattr(fileno(stdin), TCSAFLUSH, &old_flags);
	printf("\n");
}

int response_successful(struct JSONIndex* index) {
	const char* result = json_index_pair(index, "result");
	return result && !strncmp(result, "\"success\"", 9);
}

void response_print_message(struct JSONIndex* index, void(*print)(const char*, ...)) {
	const char* message = json_index_pair(index, "message");
	if (!message || json_type(message) != JSON_STRING) print_error(ERROR_RESPONSE_PARSE);
	else print("%.*s", json_string_length(message), message + 1);
}

int array_add(void** array_p, size_t size, size_t unit, const void* value_p) {
	if (size % ARRAY_REALLOC_STEP == 0) {
		void* array = realloc(*array_p, (size + ARRAY_REALLOC_STEP) * unit);
		if (!array) return 1;
		else *array_p = array;
	}
	memcpy(*array_p + size * unit, value_p, unit);
	return 0;
}

void paths_destroy(char** paths, size_t size) {
	for (size_t i = 0; i < size; i++) free(paths[i]);
	free(paths);
}

size_t paths_add(char*** paths_p, size_t size, const char* path) {
	if (*path == '.' && path[1] == '/' && path[2]) path += 2;
	struct stat statbuf;
	if (stat(path, &statbuf)) print_error("couldn't find file: %s", path);
	// dir
	else if (S_ISDIR(statbuf.st_mode)) {
		DIR* dir = opendir(path);
		if (!dir) print_error("couldn't open directory: %s", path);
		else {
			struct dirent* entry;
			while ((entry = readdir(dir))) {
				if (*entry->d_name == '.') continue;
				char* entry_path = malloc(strlen(path) + strlen(entry->d_name) + 2);
				if (!entry_path) continue;
				strcpy(entry_path, path);
				if (path[strlen(path) - 1] != '/') strcat(entry_path, "/");
				strcat(entry_path, entry->d_name);
				size = paths_add(paths_p, size, entry_path);
				free(entry_path);
				if (!*paths_p) break;
			}
			closedir(dir);
		}
	}
	// file
	else {
		char* ext = strrchr(path, '.');
		if (!ext) print_error("missing file extension: %s", path);
		else if (!api_is_extension_allowed(ext + 1)) print_error("forbidden file extension: %s", path);
		else {
			path = strdup(path);
			array_add((void*)paths_p, size, sizeof(char*), &path);
			if (!*paths_p) paths_destroy(*paths_p, size);
			else size++;
		}
	}
	return size;
}

time_t string_to_time(const char* string) {
	struct tm time;
	time.tm_mday = atoi(string += 5);
	switch (*(string += 3)) {
		case 'M': time.tm_mon = string[2] == 'r' ? 3 : 5;
		case 'A': time.tm_mon = string[1] == 'p' ? 4 : 7;
		case 'J': time.tm_mon = 5 + string[2] == 'l';
		case 'F': time.tm_mon = 2;
		case 'S': time.tm_mon = 8;
		case 'O': time.tm_mon = 9;
		case 'N': time.tm_mon = 10;
		case 'D': time.tm_mon = 11;
		default: time.tm_mon = 0;
	}
	time.tm_year = atoi(string += 4) - 1990;
	time.tm_hour = atoi(string += 5);
	time.tm_min = atoi(string += 3);
	time.tm_sec = atoi(string += 3);
	return mktime(&time);
}

int string_sort(const void* a, const void* b) {
	return strcmp(*(const char**)a, *(const char**)b);
}

/* commands */

void cmd_help(size_t argc, const char** args) {
	printf("\e[32m"
	" _ _  ___ ___  __ \n"
	"| ' \\/ -_) _ \\/ _|\e[0m v1.1\n\e[32m"
	"|_||_\\___\\___/\\__|\e[0m %s\n", rand() % 4 ? "/\\ /\\ ,o" : "thanks, webcatz!"
	);
	if (!argc) printf(
	"    \e[32minfo\e[0m [sitename]  list site info\n"
	"    \e[32mlist\e[0m [path]      list site files\n"
	"    \e[32mupload\e[0m [paths]   upload files to site\n"
	"    \e[32mdelete\e[0m [paths]   delete files from site\n"
	"    \e[32mdiff\e[0m             list changes\n"
	"    \e[32mhelp\e[0m [command]   display documentation\n\n"
	);
	else if (!strcmp(*args, "info")) printf(
	"    \e[32minfo\e[0m [sitename]\n"
	"    lists a site's name, domain, tags, creation\n"
	"    date, last update date, views, and hits.\n"
	"    [sitename] cannot exceed 32 characters.\n"
	"      if [sitename] is absent, the api key is used\n"
	"    to display its associated site.\n\n"
	);
	else if (!strcmp(*args, "list")) printf(
	"    \e[32mlist\e[0m [path]\n"
	"    lists all remote files.\n"
	"      if [path] is present, lists only the contents\n"
	"    of the remote directory at [path].\n\n"
	);
	else if (!strcmp(*args, "upload")) printf(
	"    \e[32mupload\e[0m [paths]\n"
	"    recursively uploads local files to the remote\n"
	"    root. separate multiple paths with spaces.\n"
	"      if [paths] is absent, uploads all local files.\n\n"
	);
	else if (!strcmp(*args, "delete")) printf(
	"    \e[32mdelete\e[0m [paths]\n"
	"    deletes remote files. separate multiple paths\n"
	"    with spaces.\n\n"
	);
	else if (!strcmp(*args, "diff")) printf(
	"    \e[32mdiff\e[0m\n"
	"    lists differences between local and remote\n"
	"    files, based on their paths and update times.\n\n"
	);
	else if (!strcmp(*args, "help")) printf(
	"    \e[32mhelp\e[0m [command]\n"
	"    prints documentation about a command.\n"
	"      if [command] is absent, lists all commands.\n\n"
	);
	else print_error("no documentation for: %s\n", *args);
}

/* api commands */

void cmd_info(size_t argc, const char** args) {
	// fetching info
	print_loading("directing spies");
	char key[KEY_SIZE];
	get_key(key);
	char* response = argc ? api_info(NULL, *args) : api_info(key, NULL);
	if (!response) {print_error(ERROR_RESPONSE_FETCH); return;}
	// parsing response
	struct JSONIndex* index = json_index_object(response);
	if (!index) {print_error(ERROR_ALLOCATION); goto cleanup_response;}
	if (!response_successful(index)) {response_print_message(index, print_error); goto cleanup_response;}
	// printing info
	struct JSONIndex* info = json_index_object(json_index_pair(index, "info"));
	if (!info) {print_error(ERROR_ALLOCATION); goto cleanup_response;}
	const char* buf;
	const char* sitename = NULL;
	printf("\n");
	if ((buf = json_index_pair(info, "sitename")) && json_type(buf) == JSON_STRING) {
		printf("    %.*s\n", (unsigned)json_string_length(buf), buf + 1);
		sitename = buf;
	}
	if ((buf = json_index_pair(info, "domain")) && json_type(buf) == JSON_STRING)
		printf("    \e[32mdomain\e[0m   %.*s\n", (unsigned)json_string_length(buf), buf + 1);
	else if (sitename)
		printf("    \e[32mdomain\e[0m   %.*s.neocities.org\n", (unsigned)json_string_length(sitename), sitename + 1);
	if ((buf = json_index_pair(info, "tags")) && json_type(buf) == JSON_ARRAY) {
		struct JSONIndex* tags = json_index_array(buf);
		if (!tags) print_error(ERROR_ALLOCATION);
		else {
			printf("    \e[32mtags\e[0m     ");
			for (size_t i = 0; i < json_index_size(tags); i++)
				if ((buf = json_index_item(tags, i)) && json_type(buf) == JSON_STRING) {
					if (i) printf(", ");
					printf("%.*s", (unsigned)json_string_length(buf), buf + 1);
				}
			printf("\n");
			free(tags);
		}
	}
	if ((buf = json_index_pair(info, "created_at")) && json_type(buf) == JSON_STRING)
		printf("    \e[32mcreated\e[0m  %.*s\n", (unsigned)json_string_length(buf), buf + 1);
	if ((buf = json_index_pair(info, "last_updated")) && json_type(buf) == JSON_STRING)
		printf("    \e[32mupdated\e[0m  %.*s\n", (unsigned)json_string_length(buf), buf + 1);
	if ((buf = json_index_pair(info, "views")) && json_type(buf) == JSON_INT)
		printf("    \e[32mviews\e[0m    %d\n", atoi(buf));
	if ((buf = json_index_pair(info, "hits")) && json_type(buf) == JSON_INT)
		printf("    \e[32mhits\e[0m     %d\n", atoi(buf));
	printf("\n");
	free(info);
	// cleanup
	cleanup_response: free(index); free(response);
}

void cmd_list(size_t argc, const char** args) {
	// fetching file list
	print_loading("conducting census");
	char key[KEY_SIZE];
	get_key(key);
	char* response = api_list(key, argc ? *args : NULL);
	if (!response) {print_error(ERROR_RESPONSE_FETCH); return;}
	// parsing response
	struct JSONIndex* index = json_index_object(response);
	if (!index) {print_error(ERROR_ALLOCATION); goto cleanup_response;}
	else if (!response_successful(index)) {response_print_message(index, print_error); goto cleanup_response;}
	// printing files
	struct JSONIndex* files = json_index_array(json_index_pair(index, "files"));
	if (!files) {print_error(ERROR_ALLOCATION); goto cleanup_response;}
	const char* buf;
	printf("\n");
	for (size_t i = 0; i < json_index_size(files); i++) {
		struct JSONIndex* file = json_index_object(json_index_item(files, i));
		if (!file) {print_error(ERROR_ALLOCATION); break;}
		if ((buf = json_index_pair(file, "path")) && json_type(buf) == JSON_STRING)
			printf("    %.*s", (unsigned)json_string_length(buf), buf + 1);
		if ((buf = json_index_pair(file, "is_directory")) && json_type(buf) == JSON_BOOL && json_bool(buf))
			printf("/\n");
		else
			printf("\n");
		if ((buf = json_index_pair(file, "size")) && json_type(buf) == JSON_INT)
			printf("    \e[32msize\e[0m     %d bytes\n", atoi(buf));
		if ((buf = json_index_pair(file, "created_at")) && json_type(buf) == JSON_STRING)
			printf("    \e[32mcreated\e[0m  %.*s\n", (unsigned)json_string_length(buf), buf + 1);
		if ((buf = json_index_pair(file, "updated_at")) && json_type(buf) == JSON_STRING)
			printf("    \e[32mupdated\e[0m  %.*s\n", (unsigned)json_string_length(buf), buf + 1);
		if ((buf = json_index_pair(file, "sha1_hash")) && json_type(buf) == JSON_STRING)
			printf("    \e[32msha1\e[0m     %.*s\n", (unsigned)json_string_length(buf), buf + 1);
		printf("\n");
		free(file);
	}
	free(files);
	// cleanup
	cleanup_response: free(index); free(response);
}

void cmd_upload(size_t argc, const char** args) {
	// building file list
	char** paths = NULL;
	size_t pathc = 0;
	if (!argc) pathc = paths_add(&paths, 0, ".");
	else for (size_t i = 0; i < argc; i++) {
		pathc = paths_add(&paths, pathc, args[i]);
		if (!paths) break;
	}
	if (!paths) {print_error(ERROR_ALLOCATION); return;}
	if (!pathc) {print_error(ERROR_FILE_LIST_EMPTY); goto cleanup_paths;}
	// printing file list
	print_success("found files:\n");
	for (size_t i = 0; i < pathc; i++)
		printf("    %s\n", paths[i]);
	printf("\n");
	// confirming upload
	print_input("upload these files? (y/n)");
	if (getchar() != 'y') {print_error("canceled upload"); goto cleanup_paths;}
	// uploading files
	print_loading("carrying files");
	char key[KEY_SIZE];
	get_key(key);
	char* response = api_upload(key, pathc, (const char**)paths);
	if (!response) {print_error(ERROR_RESPONSE_FETCH); goto cleanup_paths;}
	// printing response
	struct JSONIndex* index = json_index_object(response);
	if (!index) print_error(ERROR_ALLOCATION);
	else response_print_message(index, response_successful(index) ? print_success : print_error);
	// cleanup
	free(index);
	free(response);
	cleanup_paths: paths_destroy(paths, pathc);
}

void cmd_delete(size_t argc, const char** args) {
	if (!argc) {print_error("provide files"); return;}
	// confirming delete
	print_input("are you sure? (y/n)");
	if (getchar() != 'y') {print_error("canceled delete"); return;}
	// deleting files
	print_loading("letting loose");
	char key[KEY_SIZE];
	get_key(key);
	char* response = api_delete(key, argc, args);
	if (!response) {print_error(ERROR_RESPONSE_FETCH); return;}
	// printing response
	struct JSONIndex* index = json_index_object(response);
	if (!index) print_error(ERROR_ALLOCATION);
	else response_print_message(index, response_successful(index) ? print_success : print_error);
	// cleanup
	free(index);
	free(response);
}

/* utility commands */

void cmd_diff(size_t argc, const char** args) {
	print_loading("cross-referencing");
	// building local file list
	char** local_paths = NULL;
	size_t local_count = paths_add(&local_paths, 0, ".");
	if (!local_paths) {print_error(ERROR_ALLOCATION); return;}
	if (!local_count) {print_error(ERROR_FILE_LIST_EMPTY); goto cleanup_local_paths;}
	qsort(local_paths, local_count, sizeof(char*), string_sort);
	// recording local file update times
	time_t* local_times = malloc(local_count * sizeof(time_t));
	if (!local_times) {print_error(ERROR_ALLOCATION); goto cleanup_local_paths;}
	struct stat statbuf;
	for (size_t i = 0; i < local_count; i++) {
		stat(local_paths[i], &statbuf);
		local_times[i] = statbuf.st_mtime;
	}
	// fetching remote file list
	char key[KEY_SIZE];
	get_key(key);
	char* response = api_list(key, NULL);
	if (!response) {print_error(ERROR_RESPONSE_FETCH); goto cleanup_local_times;}
	// parsing response
	struct JSONIndex* index = json_index_object(response);
	if (!index) {print_error(ERROR_ALLOCATION); goto cleanup_response;}
	if (!response_successful(index)) {response_print_message(index, print_error); goto cleanup_response;}
	// recording remote files + update times
	struct JSONIndex* files = json_index_array(json_index_pair(index, "files"));
	if (!files) {print_error(ERROR_ALLOCATION); goto cleanup_response;}
	const char* buf;
	char** remote_paths = NULL;
	time_t* remote_times = NULL;
	size_t remote_count = 0;
	for (size_t i = 0; i < json_index_size(files); i++) {
		struct JSONIndex* file = json_index_object(json_index_item(files, i));
		if (!file) {print_error(ERROR_ALLOCATION); goto cleanup_response_files;}
		if ((buf = json_index_pair(file, "is_directory")) && json_type(buf) == JSON_BOOL && json_bool(buf)) continue;
		const char* buf;
		time_t time;
		char* path;
		if (!(buf = json_index_pair(file, "updated_at")) || json_type(buf) != JSON_STRING) continue;
		else time = string_to_time(buf + 1);
		if (!(buf = json_index_pair(file, "path")) || json_type(buf) != JSON_STRING) continue;
		else path = strndup(buf + 1, json_string_length(buf));
		free(file);
		if (!path) {print_error(ERROR_ALLOCATION); goto cleanup_response_files;}
		array_add((void*)&remote_times, remote_count, sizeof(time_t), &time);
		if (!remote_times) goto cleanup_remote_times;
		array_add((void*)&remote_paths, remote_count, sizeof(char*), &path);
		if (!remote_paths) goto cleanup_remote_paths;
		remote_count++;
	}
	// comparing local and remote
	print_success("local changes:\n");
	size_t local_idx = 0;
	size_t remote_idx = 0;
	while (local_idx < local_count && remote_idx < remote_count) {
		int cmp = strcmp(local_paths[local_idx], remote_paths[remote_idx]);
		if (cmp < 0) printf("\e[32m    +    %s\n", local_paths[local_idx++]);
		else if (cmp > 0) printf("\e[31m    -    %s\n", remote_paths[remote_idx++]);
		else {
			cmp = difftime(remote_times[remote_idx++], local_times[local_idx++]);
			if (cmp < 0) printf("\e[32m    + %%  %s\n", local_paths[local_idx]);
			else if (cmp > 0) printf("\e[31m    - %%  %s\n", remote_paths[remote_idx]);
		}
	}
	while (local_idx < local_count) printf("\e[32m    +    %s\n", local_paths[local_idx++]);
	while (remote_idx < remote_count) printf("\e[31m    -    %s\n", remote_paths[remote_idx++]);
	printf("\n");
	// cleanup
	cleanup_remote_times: free(remote_times);
	cleanup_remote_paths: paths_destroy(remote_paths, remote_count);
	cleanup_response_files: free(files);
	cleanup_response: free(index); free(response);
	cleanup_local_times: free(local_times);
	cleanup_local_paths: paths_destroy(local_paths, local_count);
}

/* printers with emoticon prefixes.
   these sometimes fail to print their emoticons?? not sure why */

void print_error(const char* format, ...) {
	printf("\e[31m%s\e[0m ", &":( \0:'(\0D: \0D':\0:< \0:'< \0:(c"[rand() % 7 * 4]);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
}

void print_success(const char* format, ...) {
	printf("\e[32m%s\e[0m ", &":) \0:D \0^_^\0^u^\0*O*\0:3 "[rand() % 6 * 4]);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
}

void print_loading(const char* message) {
	printf("\e[33mP:\e[0m  %s...\n", message);
}

void print_input(const char* message) {
	printf("\e[33m%s\e[0m %s ", &"<o<\0u_u\0:? \0oxo\0`u`"[rand() % 5 * 4], message);
}