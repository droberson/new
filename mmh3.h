#ifndef MMH3_H
#define MMH3_H

#include <stddef.h>
#include <stdint.h>

uint64_t  mmh3_64(const void *, const size_t, uint64_t);
uint64_t  mmh3_64_string(const char *, const uint64_t);
void      mmh3_64_make_hashes(const void *, size_t, size_t, uint64_t *);
char     *mmh3_64_hexdigest(const char *, uint64_t);
void      mmh3_128(const void *, const size_t, const uint64_t, uint64_t *);

#endif /* MMH3_H */
