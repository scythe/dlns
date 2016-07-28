
typedef struct DStr {
	char *ptr;
	size_t n;
} DStr;

char *encode_dln(char delim, char ***input);
struct DStr **decode_dln(char *input);

