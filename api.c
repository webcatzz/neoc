#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "api.h"
#include "cli.h"

#define SITENAME_MAX_LENGTH 32
#define ALLOWED_EXTENSION_COUNT 67
#define ERROR_KEY_NULL "provide api key"
#define ERROR_KEY_LENGTH "api key must be 32 characters in length"
#define ERROR_SITENAME_LENGTH "sitename cannot exceed 32 characters"

const char* allowed_extensions[ALLOWED_EXTENSION_COUNT] = {"apng", "asc", "atom", "avif", "bin", "cjs", "css", "csv", "dae", "eot", "epub", "geojson", "gif", "glb", "glsl", "gltf", "gpg", "htm", "html", "ico", "jpeg", "jpg", "js", "json", "key", "kml", "knowl", "less", "manifest", "map", "markdown", "md", "mf", "mid", "midi", "mjs", "mtl", "obj", "opml", "osdx", "otf", "pdf", "pgp", "pls", "png", "py", "rdf", "resolveHandle", "rss", "sass", "scss", "svg", "text", "toml", "ts", "tsv", "ttf", "txt", "webapp", "webmanifest", "webp", "woff", "woff2", "xcf", "xml", "yaml", "yml"};

/* curl helpers */

// adds a key authorization header to a curl string list
struct curl_slist* curl_slist_append_key(struct curl_slist* list, const char* key) {
	char header[23 + KEY_LENGTH] = "Authorization: Bearer ";
	strncat(header, key, KEY_LENGTH);
	list = curl_slist_append(list, header);
	return list;
}

// used as curl writefunction
size_t curl_response_write(char* write, size_t size, size_t byte_count, char** string_p) {
	size_t old_size = strlen(*string_p);
	size_t write_size = size * byte_count;
	char* string = realloc(*string_p, old_size + write_size + 1);
	if (!string) {
		free(*string_p);
		return 0;
	}
	else {
		*string_p = string;
		strncpy(string + old_size, write, write_size);
		return write_size;
	}
}

// performs an easy curl request and returns a string response
char* curl_request(CURL* curl) {
	char** response_ptr = malloc(sizeof(char*));
	if (!response_ptr) {print_error(ERROR_ALLOCATION); return NULL;}
	*response_ptr = malloc(1);
	if (!*response_ptr) {print_error(ERROR_ALLOCATION); free(response_ptr); return NULL;}
	**response_ptr = 0;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_ptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_response_write);

	CURLcode code = curl_easy_perform(curl);
	if (code) print_error("curl error: %s", curl_easy_strerror(code));

	char* response = *response_ptr;
	free(response_ptr);
	return response;
}

/* interface */

char* api_info(const char* key, const char* sitename) {
	if (!key && !sitename) {print_error(ERROR_KEY_NULL" or sitename"); return NULL;}
	if (key && strlen(key) != KEY_LENGTH) {print_error(ERROR_KEY_LENGTH); return NULL;}
	if (sitename && strlen(sitename) > SITENAME_MAX_LENGTH) {print_error(ERROR_SITENAME_LENGTH); return NULL;}
	// creating curl
	CURL* curl = curl_easy_init();
	if (!curl) {print_error(ERROR_ALLOCATION); return NULL;}
	// adding headers
	struct curl_slist* headers = NULL;
	if (key) {
		headers = curl_slist_append_key(headers, key);
		if (!headers) {print_error(ERROR_ALLOCATION); curl_easy_cleanup(curl); return NULL;}
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	// building url
	if (sitename) {
		char url[41 + SITENAME_MAX_LENGTH] = "https://neocities.org/api/info?sitename=";
		strncat(url, sitename, SITENAME_MAX_LENGTH);
		curl_easy_setopt(curl, CURLOPT_URL, url);
	}
	else curl_easy_setopt(curl, CURLOPT_URL, "https://neocities.org/api/info");
	// performing request
	char* response = curl_request(curl);
	// cleanup
	curl_easy_cleanup(curl);
	if (headers) curl_slist_free_all(headers);
	return response;
}

char* api_list(const char* key, const char* directory) {
	if (!key) {print_error(ERROR_KEY_NULL); return NULL;}
	if (strlen(key) != KEY_LENGTH) {print_error(ERROR_KEY_LENGTH); return NULL;}
	// creating curl
	CURL* curl = curl_easy_init();
	if (!curl) {print_error(ERROR_ALLOCATION); return NULL;}
	// adding headers
	struct curl_slist* headers = curl_slist_append_key(NULL, key);
	if (!headers) {print_error(ERROR_ALLOCATION); curl_easy_cleanup(curl); return NULL;}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	// building url
	if (directory) {
		char url[37 + strlen(directory)];
		strcpy(url, "https://neocities.org/api/list?path=");
		strcat(url, directory);
		curl_easy_setopt(curl, CURLOPT_URL, url);
	}
	else curl_easy_setopt(curl, CURLOPT_URL, "https://neocities.org/api/list");
	// performing request
	char* response = curl_request(curl);
	// cleanup
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	return response;
}

char* api_upload(const char* key, size_t filec, const char** files) {
	if (!key) {print_error(ERROR_KEY_NULL); return NULL;}
	if (strlen(key) != KEY_LENGTH) {print_error(ERROR_KEY_LENGTH); return NULL;}
	if (!filec) {print_error("provide files"); return NULL;}
	// creating curl
	CURL* curl = curl_easy_init();
	if (!curl) {print_error(ERROR_ALLOCATION); return NULL;}
	// adding headers
	struct curl_slist* headers = curl_slist_append_key(NULL, key);
	if (!headers) {print_error(ERROR_ALLOCATION); curl_easy_cleanup(curl); return NULL;}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	// adding files
	curl_mime* mime = curl_mime_init(curl);
	if (!mime) {print_error(ERROR_ALLOCATION); curl_easy_cleanup(curl); curl_slist_free_all(headers); return NULL;}
	for (size_t i = 0; i < filec; i++) {
		curl_mimepart* part = curl_mime_addpart(mime);
		if (!part) {print_error(ERROR_ALLOCATION); curl_easy_cleanup(curl); curl_slist_free_all(headers); curl_mime_free(mime); return NULL;}
		curl_mime_name(part, files[i]);
		curl_mime_filedata(part, files[i]);
	}
	curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
	// performing request
	curl_easy_setopt(curl, CURLOPT_URL, "https://neocities.org/api/upload");
	char* response = curl_request(curl);
	// cleanup
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	curl_mime_free(mime);
	return response;
}

char* api_delete(const char* key, size_t filec, const char** files) {
	if (!key) {print_error(ERROR_KEY_NULL); return NULL;}
	if (strlen(key) != KEY_LENGTH) {print_error(ERROR_KEY_LENGTH); return NULL;}
	if (!filec) {print_error("provide files"); return NULL;}
	// creating curl
	CURL* curl = curl_easy_init();
	if (!curl) {print_error(ERROR_ALLOCATION); return NULL;}
	// adding headers
	struct curl_slist* headers = curl_slist_append_key(NULL, key);
	if (!headers) {print_error(ERROR_ALLOCATION); curl_easy_cleanup(curl); return NULL;}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	// adding files
	size_t files_total_size = 0;
	for (int i = 0; i < filec; i++) files_total_size += strlen(files[i]);
	char* fields = malloc(files_total_size + 13 * filec);
	if (!fields) {print_error(ERROR_ALLOCATION); curl_easy_cleanup(curl); curl_slist_free_all(headers); return NULL;}
	*fields = 0;
	for (size_t i = 0; i < filec; i++) {
		if (i) strcat(fields, "&");
		strcat(fields, "filenames[]=");
		strcat(fields, files[i]);
	}
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields);
	// performing request
	curl_easy_setopt(curl, CURLOPT_URL, "https://neocities.org/api/delete");
	char* response = curl_request(curl);
	// cleanup
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	free(fields);
	return response;
}

/* extras */

// binary searches `allowed_extensions` for `extension`
int api_is_extension_allowed(const char* extension) {
	size_t start = 0;
	size_t end = ALLOWED_EXTENSION_COUNT - 1;
	while (start != end) {
		size_t i = start + (end - start + 1) / 2;
		if (strcmp(allowed_extensions[i], extension) > 0) end = i - 1;
		else start = i;
	}
	return !strcmp(allowed_extensions[start], extension);
}