#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "json.h"

#define INDEX_REALLOC_STEP 16

/* string helpers */

// returns a pointer to the first non-whitespace character in `json`
const char* skip_space(const char* json) {
	while (isspace(*json)) json++;
	return json;
}

// returns a pointer to just after the object/array at `json`
const char* skip_nest(const char* json) {
	unsigned nest = 1;
	while (nest) switch (*++json) {
		case '{': case '[': nest++; break;
		case '}': case ']': nest--; break;
		case '\0': return json;
	}
	return json;
}

/* type */

enum JSONType json_type(const char* json) {
	switch (*json) {
		case '{': return JSON_OBJECT;
		case '[': return JSON_ARRAY;
		case '"': return JSON_STRING;
	}
	if (isdigit(*json)) while (1) {
		if (!isdigit(*json)) return JSON_INT;
		if (*json == '.') return JSON_FLOAT;
		json++;
	}
	if (!strncmp(json, "true", 4) || !strncmp(json, "false", 5)) return JSON_BOOL;
	if (!strncmp(json, "null", 4)) return JSON_NULL;
	return JSON_UNKNOWN;
}

/* index */

struct JSONIndex {
	size_t size;
	const char* items[];
};

struct JSONIndex* json_index_object(const char* json) {
	struct JSONIndex* index = malloc(sizeof(struct JSONIndex));
	if (!index) return NULL;
	index->size = 0;
	size_t cap = 0;
	// indexing
	json = skip_space(json + 1);
	if (*json != '}') do {
		while (index->size >= cap) {
			struct JSONIndex* tmp = realloc(index, sizeof(struct JSONIndex) + (cap += INDEX_REALLOC_STEP * 2) * sizeof(char*));
			if (!tmp) {free(index); return NULL;}
			index = tmp;
		}
		// key
		json = skip_space(json);
		if (*json != '"') break;
		index->items[index->size++] = json;
		json += json_string_length(json) + 1;
		if (!*json) break;
		json = skip_space(json + 1);
		if (*json != ':') break;
		// value
		json = skip_space(json + 1);
		if (!*json) break;
		index->items[index->size++] = json;
		if (*json == '{' || *json == '[') json = skip_nest(json);
		else if (*json == '"') json += json_string_length(json) + 1;
		json = strpbrk(json, ",]");
		if (!json) break;
	} while (*json++ == ',');
	return index;
}

struct JSONIndex* json_index_array(const char* json) {
	struct JSONIndex* index = malloc(sizeof(struct JSONIndex));
	if (!index) return NULL;
	index->size = 0;
	size_t cap = 0;
	// indexing
	json = skip_space(json + 1);
	if (*json != ']') do {
		while (index->size >= cap) {
			struct JSONIndex* tmp = realloc(index, sizeof(struct JSONIndex) + (cap += INDEX_REALLOC_STEP) * sizeof(char*));
			if (!tmp) {free(index); return NULL;}
			index = tmp;
		}
		// value
		json = skip_space(json);
		if (!*json) break;
		index->items[index->size++] = json;
		if (*json == '{' || *json == '[') json = skip_nest(json);
		else if (*json == '"') json += json_string_length(json) + 1;
		json = strpbrk(json, ",]");
		if (!json) break;
	} while (*json++ == ',');
	return index;
}

const char* json_index_pair(const struct JSONIndex* index, const char* key) {
	for (size_t i = 0; i < index->size; i += 2)
		if (!strncmp(key, index->items[i] + 1, strlen(key)))
			return i + 1 == index->size ? NULL : index->items[i + 1];
	return NULL;
}

const char* json_index_item(const struct JSONIndex* index, size_t pos) {
	return index->items[pos];
}

size_t json_index_size(const struct JSONIndex* index) {
	return index->size;
}

/* value */

size_t json_string_length(const char* json) {
	size_t length = 0;
	int escape = 0;
	while (*++json && (*json != '"' || escape)) {
		escape = *json == '\\' ? !escape : 0;
		length++;
	}
	return length;
}

int json_bool(const char* json) {
	return *json == 't';
}