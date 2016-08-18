#include "dln.h"

struct newline {
	char at;
	char prev; // '\0' indicates not present
};

static struct newline read_newline(char *nl_str)
{
	struct newline ret = {'\n', '\0'};
	if ('\n' == *nl_str)
		return ret;
	else if (nl_str[1] == '\n' && ((ret.prev = '\r') == *nl_str))
		return ret;
	else if ((ret.at = '\r') == *nl_str)
		return ret;
	ret.at = '\0';
	return ret;
}

static int is_newline(char *pos, struct newline nl)
{
	return (pos[0] == nl.at) && (!nl.prev || pos[-1] == nl.prev);
}

static int invalid_delim(char delim)
{
	switch(delim) {
	case '/': case ';': case ':': case '|': case '\t': case ',': case '.':
		return 0;
	default:
		return EILSEQ;
	}
}

static unsigned int ihash(size_t key, int type)
{
#define ROT(y, n) (((y << n) | (y >> (6-n))) & 63)
	static const unsigned int VALS[3] = {3141592653u, 2718281829u, 577215665u};
	unsigned int hash = 0, chunk, i;
	key *= type ? VALS[type - 1] : 1;
	for (; key; key >>= 6) {
		chunk = key & 63;
		chunk = ROT(chunk, 3);
		chunk *= 37;
		chunk = ROT(chunk, 4);
		hash ^= chunk;
	}
	hash = hash ^ (hash >> 2);
	hash = (hash * 19) & 63;
	hash = hash ^ (hash >> 3);
	return hash;
#undef ROT
}

static unsigned long fast_triangle_root(unsigned long x)
{
	float y = (float) 2*x;
	unsigned long yi = * (unsigned long *) &y;
	bool onepointfour = !!(yi & 1<<23);
	yi = (yi & ~(1<<23)) >> 1; //XXX wrong -- fix
	y = * (float *) yi;
	y *= onepointfour ? 1.41 : 1;
	return y / 2 + x / y;
}

#define TRAVERSE(strin, catch_bracket, catch_default)	\
{							\
	char cur_bracket;				\
	size_t equals_len;				\
	for (; *strin; strin++) {			\
		switch(*strin) {			\
		case '[':				\
		case ']':				\
			/*found a quote of length "equals_len"	*/ \
			if (cur_bracket == *strin) { 	\
				catch_bracket;		\
				/*allow odd-numbered sequences of	\
				 *brackets to end in single bracket */	\
				cur_bracket = equals_len && cur_bracket;\
			} else {			\
				cur_bracket = *strin;	\
				/*don't detect single brackets in even-	\
				 *-numbered sequences of brackets */	\
				if (strin[1] != cur_bracket) {	\
					equals_len = -1;\
					catch_bracket;	\
				}			\
			}				\
			equals_len = 0;			\
			break;				\
		case '=':				\
			if (cur_bracket)		\
				equals_len++;		\
			break;				\
		default: 				\
			catch_default;		\
			equals_len = 0;		\
			break;			\
		}			\
	}				\
}

static size_t longquote(char delim, struct newline nl, char *str) {
	unsigned long long found_quotes = 0x0, bloom[4];
	char *found_vals, *start = str;
	size_t i, j, halfmax;
	bool found_delim = false;
	TRAVERSE(str, {
	               equals_len++; // equals_len = -1 for single [ or ]
	               if (equals_len < 64)
	                  found_quotes |= 1 << equals_len;
	               else
	                  for (i = 0; i < 4; ++i)
	                     bloom[i] |= 1 << ihash(equals_len, i);
	              }, { found_delim = found_delim ?: (
	                   *str == delim || is_newline(str, nl)); });
	if (!found_delim)
		return 0;

	for (i = 0; i < 64; ++i)
		if (!((found_quotes << i) & 1))
			return i + 1;

	halfmax = fast_triangle_root((size_t) (str - start));
	str = start;
	for (i = 64; i < halfmax; ++i)
		for (j = 0; j < 4; ++j)
			if (!(bloom[j] & (1 << ihash(i, j)))) //length 'i' is not in the filter
				return i + 1;

	//Okay, at this point we know we're being DOS'ed
	found_vals = calloc(1, halfmax / 8 + 1);
	TRAVERSE(str, {
	               if (equals_len >= halfmax && equals_len < 2 * halfmax)
	                  found_vals[(equals_len - halfmax) / 8] |= 1 << ((equals_len - halfmax) % 8);
	              }, );
	for (i = 0; i < halfmax / 8 + 1; ++i)
		for (j = 0; j < 8; ++j) 
			if (!(1 << j & found_vals[i]))
				return halfmax + i * 8 + j;

	return str - start; //should never happen
}

static size_t write_quote(char *stream, size_t len, bool left)
{
	if (len) {
		size_t end = len - 1;
		stream[0] = stream[end] = left ? '[' : ']';
		while (0 < (end -= 1))
			stream[end] = '=';
	}
	return len;
}

//static void APPEND('T *buf, 'T obj, size_t sz);
#define APPEND(buf, obj, sz) {					\
	if (sz > 7 && !(sz&(sz-1))) {				\
		void *nbuf = realloc(buf, sizeof(*buf) * sz);	\
		if (!nbuf) return NULL;				\
		buf = (typeof(buf)) nbuf;			\
	}							\
	buf[sz] = (typeof(*buf)) obj;				\
}

#define APPEND_CHECK(buf, obj_expr, sz) {	\
	typeof(*buf) obj = obj_expr;		\
	if (!obj) return NULL;			\
	APPEND(buf, obj, sz)			\
}

char *encode_dln(char delim, char *newline, char ***items) {
	size_t i, j, **quotes, len = 2;
	char *ret, *lq = NULL, *rq = NULL;
	struct newline nl = read_newline(newline);
	if (!nl.at || invalid_delim(delim))
		return NULL;

	quotes = calloc(8, sizeof(*quotes));
	for (i = 0; items[i]; ++i) {
		APPEND_CHECK(quotes, malloc(sizeof(*quotes)), i);
		quotes[i] = calloc(8, sizeof(*quotes[i]));
		for (j = 0; items[i][j]; ++j) {
			APPEND(quotes[i], 0, j);
			quotes[i][j] = longquote(delim, nl, items[i][j]);
			len += strlen(items[i][j]) + 2 * quotes[i][j] + 1;
		}
	}	
	ret = calloc(1, len + 1);
	len = 0;
	for (i = 0; items[i]; ++i)
		for (j = 0; items[i][j]; ++j) {
			if (len) {
				strncpy(ret + len, &delim, 1);
				len += 1;
			}
			len += write_quote(ret + len, quotes[i][j], true);
			strcpy(ret + len, items[i][j]);
			len += strlen(items[i][j]);
			len += write_quote(ret + len, quotes[i][j], false);
		}
	
	return ret;
}

static DStr read_longstring(char *ls)
{
	size_t quote_len = 2;
	DStr elem = {NULL, 0};
	if (ls[1] != '[' && ls[1] != '=') {
		quote_len = 1;
	} else {
		for (; ls[1] == '='; ++ls, ++quote_len);
		if (ls[1] != '[') {
			return elem;
		}
	}
	elem.ptr = &ls[2];
	TRAVERSE(elem.ptr, { 
		if (equals_len + 2 == quote_len && cur_bracket == ']')
			elem.n = elem.ptr - ls - quote_len - 1;
		return elem; }, );
	elem.ptr = NULL;
	return elem;
}

DStr **decode_dln(char *input)
{
	size_t loc, line = 0, line_len = 0;
	DStr **lines = malloc(8 * sizeof(DStr *));
	char delim = input[0], *last;
	DStr elem, null_dstr = {NULL, 0};
	struct newline nl = read_newline(&input[1]);
	if (!nl.at || invalid_delim(delim)) {
		return NULL;
	}
	input += 2;
	for (loc = 0; input[loc]; ++loc) {
		if (last == &input[loc-1]) {
			if (input[loc] == '[') { //if longstring
				elem = read_longstring(&input[loc]);
				if (!elem.ptr)
					return NULL;
				APPEND(lines[line-1], elem, line_len);
				line_len += 1;
				for(; !is_newline(&input[loc], nl) &&
				      input[loc] != delim; ++loc);
			} else if (input[loc] == ']' && *last == nl.at) {
				while(!is_newline(&input[loc], nl))
					++loc;
				last = &input[loc];
			}
		} else if (is_newline(&input[loc], nl)) {
			APPEND(lines[line-1], null_dstr, line_len);
			APPEND_CHECK(lines, malloc(8 * sizeof(DStr)), line);
			line += 1;
			line_len = 0;
			goto Delim;
		} else if (input[loc] == delim) {
Delim:
			elem.ptr = &last[1];
			last = &input[loc];
			elem.n = (size_t) (last - elem.ptr);
			APPEND(lines[line-1], elem, line_len);
			line_len += 1;
		}
	}
	return lines;
}

