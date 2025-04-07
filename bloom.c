#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "mmh3.h"
#include "bloom.h"

static const char *bloom_errors[] = {
	"Success",
	"Out of memory",
	"Unable to open file",
	"Unable to read file",
	"Unable to write to file",
	"fstat() failure",
	"Invalid file format"
};

static size_t ideal_size(const size_t expected, const float accuracy) {
	return -(expected * log(accuracy) / pow(log(2.0), 2));
}

bloom_error_t bloom_init(bloomfilter *bf, const size_t expected, const float accuracy, const size_t max_stacks) {
	bf->base_size     = ideal_size(expected, accuracy);
	bf->stack_count   = 1;
	bf->max_stacks    = max_stacks;
	bf->needs_rebuild = false;
	bf->size          = bf->base_size * bf->stack_count;
	bf->hashcount     = (bf->size / expected) * log(2);
	bf->bitmap_size   = bf->size / 8;
	bf->expected      = expected;
	bf->accuracy      = accuracy;
	bf->insert_count  = 0;
	bf->bitmap        = calloc(bf->bitmap_size, sizeof(uint8_t));
	if (bf->bitmap == NULL) {
		return BF_OUTOFMEMORY;
	}

	return BF_SUCCESS;
}

void bloom_destroy(bloomfilter *bf) {
	if (bf->bitmap) {
		free(bf->bitmap);
		bf->bitmap = NULL;
	}
}

// This assumes that the filter is sized appropriately.
bool bloom_populate_from_file(bloomfilter *bf, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("fopen");
        return false;
    }

    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, fp) != -1) {
        // strip trailing newline
        size_t read = strlen(line);
        if (read > 0 && line[read - 1] == '\n') {
            line[--read] = '\0';
        }

        bloom_lookup_or_add_string(bf, line);
    }

    free(line);
    fclose(fp);
    return true;
}

static inline void calculate_positions(uint64_t position, uint64_t *byte_position, uint8_t *bit_position) {
	*byte_position = position / 8;
	*bit_position = position % 8;
}

bool bloom_lookup_or_add(bloomfilter *bf, const void *element, const size_t len) {
	//fprintf(stderr, "DEBUG: base_size=%zu stack_count=%zu size=%zu bitmap_size=%zu hashcount=%zu insert_count=%zu\n", bf->base_size, bf->stack_count, bf->size, bf->bitmap_size, bf->hashcount, bf->insert_count);
    uint64_t hashes[bf->hashcount];
    mmh3_64_make_hashes(element, len, bf->hashcount, hashes);

    // search all stacks
    for (size_t stack = 0; stack < bf->stack_count; stack++) {
        bool found = true;

        for (size_t i = 0; i < bf->hashcount; i++) {
            size_t offset = stack * bf->base_size;
            size_t result = hashes[i] % bf->base_size + offset;
            size_t byte_position = result / 8;
            uint8_t bit_position = result % 8;

            if ((bf->bitmap[byte_position] & (1 << bit_position)) == 0) {
                found = false;
                break;
            }
        }

        if (found) {
			return true; // already seen
		}
    }

    // insert into the last stack
    size_t offset = (bf->stack_count - 1) * bf->base_size;
    for (size_t i = 0; i < bf->hashcount; i++) {
        size_t result = hashes[i] % bf->base_size + offset;
        size_t byte_position = result / 8;
        uint8_t bit_position = result % 8;

        bf->bitmap[byte_position] |= (1 << bit_position);
    }

    bf->insert_count++;

	if (bf->insert_count >= bf->expected) {
		if (bf->max_stacks == 0 || bf->stack_count < bf->max_stacks) {
			//fprintf(stderr, "stacking... stack count: %ld expected: %ld\n", bf->stack_count, bf->expected);
			if (!bloom_stack(bf)) {
				fprintf(stderr, "Failed to stack bloom filter\n");
				// TODO fail or raise error somehow?
			}
		}
	}

	if (bf->max_stacks != 0 && bf->stack_count >= bf->max_stacks) {
		bf->needs_rebuild = true;
	}

    return false;
}

bool bloom_lookup_or_add_string(bloomfilter *bf, const char *element) {
	return bloom_lookup_or_add(bf, element, strlen(element));
}

bool bloom_stack(bloomfilter *bf) {
    bf->stack_count++;
    size_t new_size_bits = bf->base_size * bf->stack_count;
    size_t new_size_bytes = new_size_bits / 8;

    uint8_t *new_bitmap = realloc(bf->bitmap, new_size_bytes);
    if (!new_bitmap) {
		return false;
	}

    // zero the new portion
    memset(new_bitmap + bf->bitmap_size, 0, new_size_bytes - bf->bitmap_size);

    bf->bitmap = new_bitmap;
    bf->size = new_size_bits;
    bf->bitmap_size = new_size_bytes;
    bf->insert_count = 0;

    return true;
}

bloom_error_t bloom_save(const bloomfilter *bf, const char *path) {
	FILE             *fp;
	bloomfilter_file  bff = {0};

	bff.magic[0] = '!';
	bff.magic[1] = 'b';
	bff.magic[2] = 'l';
	bff.magic[3] = 'o';
	bff.magic[4] = 'o';
	bff.magic[5] = 'm';
	bff.magic[6] = 'z';
	bff.magic[7] = '!';

	bff.size         = bf->size;
	bff.hashcount    = bf->hashcount;
	bff.bitmap_size  = bf->bitmap_size;
	bff.expected     = bf->expected;
	bff.accuracy     = bf->accuracy;
	bff.insert_count = bf->insert_count;
	bff.base_size    = bf->base_size;
	bff.stack_count  = bf->stack_count;
	bff.max_stacks   = bf->max_stacks;
	bff.ino          = bf->ino;
	bff.dev          = bf->dev;
	bff.mtime        = bf->mtime;

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return BF_FOPEN;
	}

	if (fwrite(&bff, sizeof(bloomfilter_file), 1, fp) != 1 ||
		fwrite(bf->bitmap, bf->bitmap_size, 1, fp) != 1) {
		fclose(fp);
		return BF_FWRITE;
	}

	fclose(fp);
	return BF_SUCCESS;
}

bloom_error_t bloom_load(bloomfilter *bf, const char *path) {
	FILE             *fp;
	struct stat       sb;
	bloomfilter_file  bff;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return BF_FOPEN;
	}

	if (fstat(fileno(fp), &sb) == -1) {
		fclose(fp);
		return BF_FSTAT;
	}

	if (fread(&bff, sizeof(bloomfilter_file), 1, fp) != 1) {
		fclose(fp);
		return BF_FREAD;
	}

	bf->size         = bff.size;
	bf->hashcount    = bff.hashcount;
	bf->bitmap_size  = bff.bitmap_size;
	bf->expected     = bff.expected;
	bf->accuracy     = bff.accuracy;
	bf->insert_count = bff.insert_count;
	bf->base_size    = bff.base_size;
	bf->stack_count  = bff.stack_count;
	bf->max_stacks   = bff.max_stacks;
	bf->ino          = bff.ino;
	bf->dev          = bff.dev;
	bf->mtime        = bff.mtime;

	bf->needs_rebuild = false;

	// check if filter has changed on disk. if so, the filter cannot be trusted and must be rebuilt

	// basic sanity check. should fail if filter isn't valid
	if ((bf->size / 8) != bf->bitmap_size ||
		sizeof(bloomfilter_file) + bf->bitmap_size != sb.st_size) {
		fclose(fp);
		return BF_INVALIDFILE;
	}

	bf->bitmap = malloc(bf->bitmap_size);
	if (bf->bitmap == NULL) {
		fclose(fp);
		return BF_OUTOFMEMORY;
	}

	if (fread(bf->bitmap, bf->bitmap_size, 1, fp) != 1) {
		fclose(fp);
		free(bf->bitmap);
		bf->bitmap = NULL;
		return BF_FREAD;
	}

	fclose(fp);

	return BF_SUCCESS;
}

const char *bloom_strerror(bloom_error_t error) {
	if (error < 0 || error >= BF_ERRORCOUNT) {
		return "Unknown error";
	}

	return bloom_errors[error];
}
