/* api interface */

#define KEY_LENGTH 32

/* api methods.
   all functions return a json string response, or null if the method failed.
   see https://neocities.org/api for more information on the api methods */

// either `key` or `sitename` may be null, but not both.
char* api_info(const char* key, const char* sitename);

// `directory` may be null.
char* api_list(const char* key, const char* directory);

char* api_upload(const char* key, size_t filec, const char** files);

char* api_delete(const char* key, size_t filec, const char** files);

/* extras */

// for non-supporter accounts, neocities only allows a selection of file formats.
// returns 1 if `extension` is an allowed file extension, otherwise 0
int api_is_extension_allowed(const char* extension);