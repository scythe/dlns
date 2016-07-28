

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
	char cur_bracket, *_str = _strin;		\
	size_t equals_len;				\
	for (; *_str; _str++) {				\
		switch(*_str) {				\
		case '[':				\
		case ']':				\
			//found a quote of length "equals_len"	\
			if (cur_bracket == *_str) { 	\
				catch_bracket;		\
				/*allow odd-numbered sequences of	\
				 *brackets to end in single bracket */	\
				cur_bracket = equals_len && cur_bracket;\
			} else {			\
				cur_bracket = *_str;	\
				/*don't detect single brackets in even-	\
				 *-numbered sequences of brackets */	\
				if (_str[1] != cur_bracket) {	\
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

static size_t longquote(char *str) {
	unsigned long long found_quotes = 0x0, bloom[4];
	char *found_vals;
	size_t i, j, halfmax;
	bool found_delim = false;
	TRAVERSE(str, {
	               equals_len++; // equals_len = -1 for single [ or ]
	               if (equals_len < 64)
	                  found_quotes[equals_len] |= 1 << equals_len;
	               else
	                  for (i = 0; i < 4; ++i)
	                     bloom[i] |= 1 << ihash(equals_len, i);
	              }, { found_delim = found_delim ?: *str == delim; });
	if (!found_delim)
		return 0;

	for (i = 0; i < 64; ++i)
		if (!((found_quotes << i) & 1))
			return i + 1;

	halfmax = fast_triangle_root(total_len);
	for (i = 64; i < halfmax; ++i)
		for (j = 0; j < 4; ++j)
			if (!(bloom[j] & (1 << ihash(i, j)))) //length 'i' is not in the filter
				return i + 1;

	//Okay, at this point we know we're being DOS'ed
	str -= total_len;
	found_vals = calloc(halfmax / 8 + 1);
	TRAVERSE(str, {
	               if (equals_len >= halfmax && equals_len < 2 * halfmax)
	                  found_vals[(equals_len - halfmax) / 8] |= 1 << ((equals_len - halfmax) % 8);
	              }, );
	for (i = 0; i < halfmax / 8 + 1; ++i) {
		for (j = 0; j < 8; ++j) 
			if (!(1 << j & found_vals[i]))
				return halfmax + i * 8 + j;

	return total_len; //should never happen
}

static void write_quote(char *stream, size_t len, bool left)
{
	if (len) {
		size_t end = len - 1;
		stream[0] = stream[end] = left ? '[' : ']';
		while (0 < (end -= 1))
			stream[end] = '=';
	}
	return len;
}
	

char *encode_dln(char delim, char ***items, size_t *rowlens, size_t numrows)
{
	size_t i, j, ***quotes, len = 2;
	char *ret, *lq = NULL, *rq = NULL;

	for (i = 0; i < numrows; ++i)
		for (j = 0; j < rowlens[i]; ++j)
			len += strlen(itmes[i][j]) + (quotes[i][j] = longquote(items[i][j])) * 2;
		
	ret = calloc(len + 1);
	len = 0;
	for (i = 0; i < numrows; ++i)
		for (j = 0; j < rowlens[i]; ++j) {
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
