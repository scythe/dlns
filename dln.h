#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

typedef struct DStr {
	char *ptr;
	size_t n;
} DStr;

char *encode_dln(char delim, char *newline, char ***input);
DStr **decode_dln(char *input);

