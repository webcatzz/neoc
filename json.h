/* lightweight json parser
   works directly off a source string */

enum JSONType {
	JSON_UNKNOWN,
	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_FLOAT,
	JSON_INT,
	JSON_BOOL,
	JSON_NULL,
};

// returns the type of the value at `json` as a jsontype constant
enum JSONType json_type(const char* json);

/* index functions
   an index contains a list of pointers into a source string
   each pointer points to a json value
	 may break if the source string is ever modified
	 must be freed after use */

struct JSONIndex;

// returns an index of values in the array at `json`
struct JSONIndex* json_index_array(const char* json);

// returns an index of keys and values in the object at `json`
struct JSONIndex* json_index_object(const char* json);

// returns the value at some position in an index
// assumes the position is less than the index size
const char* json_index_item(const struct JSONIndex* index, size_t position);

// returns the value after some key in an index
// if no such value exists, returns null
// assumes values at even positions in the index are strings
const char* json_index_pair(const struct JSONIndex* index, const char* key);

// returns the number of values in an index
// an object index's size includes both keys and values
size_t json_index_size(const struct JSONIndex* index);

/* helper functions for retreiving values from a string
   `json` is assumed to point to a value of the relevant type
	 for other types, use standard c functions */

// returns the length of the string at `json`
size_t json_string_length(const char* json);

// returns the bool at `json` as 1 (true) or 0 (false)
int json_bool(const char* json);