#include <string.h>
#include <assert.h>

/**
 * 
 * Implementation detail: args is not being bounds-checked.
 * @param fmtStr Pattern string - Use $n placeholders where (mandatory) n is the positional arg index and (one char limit) n e [0,9] (u[A,Z]u[a,z] - not yet implemented). Escape $ using $$.
 **/
int ffmt(const char * fmtStr, int (*charArrayConsumer)(const char * const, const size_t), const char * const args[]) {
	for (int i = 0; ; ++i) {
		switch (fmtStr[i]) {
			default:
				//nop
	continue;
			case '$': {
				++i;
				// lookahead...
				char nextchar = fmtStr[i];
				if (nextchar == '$') {
					// To branch or to ternary - I think readability is better this way.
					// We need a condition for printing something from args anyway.
					// Would have needed a fputc - or charArrayConsumer({'$'}, 1) - if escaping didn't work in a postfix-mode.
					int ret = charArrayConsumer(fmtStr, i);
					if (ret == -1) {
return ret;
					}
				} else {
					int ret = charArrayConsumer(fmtStr, i-1);
					if (ret == -1) {
return ret;
					}
					// pucgenie: Feel free to implement the remaining parts of the spec
					size_t posidx = nextchar - '0';
					assert(posidx >= 0 && posidx < 10);

					const char * const posbuf = args[posidx];
					ret = charArrayConsumer(posbuf, strlen(posbuf));
					if (ret == -1) {
return ret;
					}
				}
				// move window beginning
				fmtStr += i + 1;
				i = 0;
			}
	continue;
			case '\0':
				// Is there something left in the buffer?
				if (i > 1) {
					// pucgenie: I don't get it why there mustn't be expressed i-1
					int ret = charArrayConsumer(fmtStr, i);
					if (ret == -1) {
return ret;
					}
				}
return 0;
		}
	}
	// endless loop
	//return -1;

}
