/* cli commands + printers */

#define ERROR_ALLOCATION "couldn't allocate memory (%s:%d)", __FILE__, __LINE__

/* commands. */

// usage: help [command]
// prints documentation about a command
// if [command] is absent, lists all commands
void cmd_help(size_t argc, const char** args);

// usage: info [sitename]
// lists a site's name, domain, tags, creation date, last update date, views, and hits
// [sitename] cannot exceed 32 characters
// if [sitename] is absent, the api key is used to display its associated site
void cmd_info(size_t argc, const char** args);

// usage: list [path]
// lists all remote files
// if [path] is present, lists only the contents of the remote directory at [path]
void cmd_list(size_t argc, const char** args);

// usage: upload [paths]
// recursively uploads local files to the remote root. separate multiple paths with spaces; exclude paths by prefixing them with '-'
// if [paths] is absent, uploads all local files
void cmd_upload(size_t argc, const char** args);

// usage: delete [paths]
// deletes remote files. separate multiple paths with spaces
void cmd_delete(size_t argc, const char** args);

// usage: diff
// lists differences between local and remote files, based on their paths and update times
void cmd_diff(size_t argc, const char** args);

/* printers */

void print_error(const char* format, ...);

void print_success(const char* format, ...);

void print_loading(const char* message);

void print_input(const char* message);