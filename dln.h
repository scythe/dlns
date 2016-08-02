
typedef struct Dstr {
	char *ptr;
	size_t n;
} Dstr;

char *encode_dln(char delim, char ***input);
Dstr **decode_dln(char *input);

