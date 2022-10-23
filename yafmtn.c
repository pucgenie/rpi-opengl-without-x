/**
 * To copy or to point to ("reference")...
**/
struct nstring {
    const char * str,
    const size_t len,
}
/**
 *
**/
static int fnfmt(const struct nstring fmtStr, int (*charArrayConsumer)(const char * const, const size_t), const struct nstring args[]) {
    for (int i = 0, j = 0; ; ++i) {
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
                    int ret = charArrayConsumer(fmtStr + j, i);
                    if (ret == -1) {
return ret;
                    }
                } else {
                    int ret = charArrayConsumer(fmtStr + j, i-1);
                    if (ret == -1) {
return ret;
                    }
                    // Feel free to implement the remaining parts of the spec
                    size_t posidx = nextchar - '0';
                    assert(posidx >= 0 && posidx < 10);

                    const char * const posbuf = args[posidx];
                    ret = charArrayConsumer(posbuf, strlen(posbuf));
                    if (ret == -1) {
return ret;
                    }
                }
                ++i;
                j = i;
            }
    continue;
            case '\0':
                // Is there something left in the buffer?
                if (i > 1) {
                    // pucgenie: I don't get it why there mustn't be expressed i-1
                    int ret = charArrayConsumer(fmtStr + j, i);
                    if (ret == -1) {
return ret;
                    }
                }
return 0;
        }
    }

}