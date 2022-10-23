/**
 * 
 * Implementation detail: args is not being bounds-checked.
 * @param fmtStr Pattern string - Use $n placeholders where (mandatory) n is the positional arg index and (one char limit) n e [0,9] (u[A,Z]u[a,z] - not yet implemented). Escape $ using $$.
 **/
int ffmt(const char * fmtStr, int (*charArrayConsumer)(const char * const, const size_t), const char * const args[]);