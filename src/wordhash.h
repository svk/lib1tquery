#ifndef H_WORDHASH
#define H_WORDHASH

typedef void* WordHashCtx;

WordHashCtx read_wordhashes(const char*);
void free_wordhashes(WordHashCtx);

int lookup_wordhash( WordHashCtx, const char * );

#endif
