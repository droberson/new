#ifndef BLOOM_H
#define BLOOM_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
	BF_SUCCESS = 0,
	BF_OUTOFMEMORY,
	BF_FOPEN,
	BF_FREAD,
	BF_FWRITE,
	BF_FSTAT,
	BF_INVALIDFILE,
	// ERRORCOUNT is used as a counter. do not add anything below this line.
	BF_ERRORCOUNT
} bloom_error_t;

typedef struct {
	size_t   size;
	size_t   base_size; // size of one stacked segment
	size_t   hashcount;
	size_t   bitmap_size;
	size_t   expected;
	float    accuracy;
	size_t   insert_count;
	size_t   max_stacks;
	size_t   stack_count;  // how many stacks are currently active
	size_t   needs_rebuild;
	uint64_t ino;
	uint64_t dev;
	uint64_t mtime;
	uint8_t *bitmap;
} bloomfilter;

typedef struct {
	uint8_t  magic[8];
	uint64_t size;
	uint64_t base_size;
	uint64_t hashcount;
	uint64_t bitmap_size;
	uint64_t expected;
	float    accuracy;
	uint64_t insert_count;
	uint64_t stack_count;
	uint64_t max_stacks;
	uint64_t ino;
	uint64_t dev;
	uint64_t mtime;
} bloomfilter_file;

bloom_error_t  bloom_init(bloomfilter *, const size_t, const float, const size_t);
void           bloom_destroy(bloomfilter *);
const char    *bloom_strerror(const bloom_error_t);
bloom_error_t  bloom_save(const bloomfilter *, const char *);
bloom_error_t  bloom_load(bloomfilter *, const char *);
bool           bloom_populate_from_file(bloomfilter *, const char *);
bool           bloom_stack(bloomfilter *);
bool           bloom_lookup_or_add(bloomfilter *, const void *, const size_t);
bool           bloom_lookup_or_add_string(bloomfilter *, const char *);

#endif /* BLOOM_H */
