#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include "bloom.h"
#include "mmh3.h"

#define DEFAULT_INITIAL_SIZE        10000
#define DEFAULT_MAX_STACKS              5
#define LARGE_FILE_THRESHOLD (100 * 1024) // 100 Kb

void usage(const char *progname) {
	fprintf(stderr,
			"usage: %s [options] [file]\n"
			"options:\n"
			"  -s SIZE    Initial filter capacity (default %d)\n"
			"  -m COUNT   Maximum number of filter stacks (default %d)\n"
			"  -f         Force filter rebuild\n"
			"  -v         Verbose output\n"
			"  -n         Do not save cache files in ~/.new\n"
			"  -h         Help\n"
			"\n"
			"If no file is specified, deduplicate stdin stream to stdout\n",
			progname, DEFAULT_INITIAL_SIZE, DEFAULT_MAX_STACKS);
}

const char *get_home_dir(void) {
	const char *home = getenv("HOME");
	if (home) return home;
	struct passwd *pw = getpwuid(getuid());
	return pw ? pw->pw_dir : NULL;
}

int check_cache_dir(void) {
    const char *home = get_home_dir();
    if (!home) {
		fprintf(stderr, "Could not determine home directory\n");
		return -1;
	}

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/.new", home);

    struct stat st;
    if (stat(path, &st) == -1) {
		if (mkdir(path, 0700) == -1) {
			perror("mkdir ~/.new");
			return -1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "~/.new exists but is not a directory\n");
        return -1;
    }

	// enforce 0700 permissions on directory
	if ((st.st_mode & 0777) != 0700) {
		if (chmod(path, 0700) == -1) {
			perror("chmod ~/.new");
			return -1;
		}
	}

    return 0;
}

int get_cache_path(const char *filepath, char *out_path, size_t out_size) {
    uint64_t hash = mmh3_64_string(filepath, 0);
    const char *home = get_home_dir();

    if (!home) {
			return -1;
	}

    snprintf(out_path, out_size, "%s/.new/%016llx", home, (unsigned long long)hash);

    return 0;
}

size_t count_lines(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
		return 0;
	}

    size_t count = 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '\n') {
			count++;
		}
    }

    fclose(fp);

    return count ? count : DEFAULT_INITIAL_SIZE;
}

bool is_large_file(const char *path) {
    struct stat st;

    if (stat(path, &st) == -1) {
		return false;  // file doesn't exist.
	}

    return st.st_size > LARGE_FILE_THRESHOLD;
}

bool stat_mismatch(const bloomfilter *bf, const char *filepath) {
	struct stat st;

	if (stat(filepath, &st) == -1) {
		return true;
	}

	return bf->ino != st.st_ino || bf->dev != st.st_dev || bf->mtime != st.st_mtime;
}

void update_stat_metadata(bloomfilter *bf, const char *filepath) {
	struct stat st;

	if (stat(filepath, &st) == 0) {
		bf->ino = st.st_ino;
		bf->dev = st.st_dev;
		bf->mtime = st.st_mtime;
	}
}

int main(int argc, char *argv[]) {
	int          opt;
	int          initial_size = DEFAULT_INITIAL_SIZE;
	size_t       max_stacks = DEFAULT_MAX_STACKS;
	const char  *filepath = NULL;
	char         resolved_path[PATH_MAX * 2];
	bool         force_rebuild = false;
	bool         verbose = false;
	bool         stdin_mode = false;
	bool         no_cache = false;
	char         cache_path[PATH_MAX] = {0};
	bool         have_cache = false;
	FILE        *out = NULL;
	bloomfilter  bf;

	while ((opt = getopt(argc, argv, "s:m:fvnh")) != -1) {
		switch(opt) {
		case 's':
			initial_size = atoi(optarg);
			break;
		case 'm':
			max_stacks = atoi(optarg);
			break;
		case 'f':
			force_rebuild = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 'n':
			no_cache = true;
			break;
		case 'h':
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	// set up output stream and cache file
	if (optind < argc) {
		char dirbuf[PATH_MAX];
		char filebuf[PATH_MAX];
		char resolved_dir[PATH_MAX];

		snprintf(dirbuf, sizeof(dirbuf), "%s", argv[optind]);
		snprintf(filebuf, sizeof(filebuf), "%s", argv[optind]);

		char *dir = dirname(dirbuf);
		char *file = basename(filebuf);

		if (!realpath(dir, resolved_dir)) {
			perror("realpath (directory)");
			return EXIT_FAILURE;
		}

		snprintf(resolved_path, sizeof(resolved_path), "%s/%s", resolved_dir, file);
		filepath = resolved_path;
	} else {
		stdin_mode = true;
	}

	if (!stdin_mode && (out = fopen(filepath, "a")) == NULL) {
		perror("fopen()");
		return EXIT_FAILURE;
	}

	if (stdin_mode) {
		out = stdout;
	}

	if (!no_cache && !stdin_mode) {
		if (check_cache_dir() != 0) {
			return EXIT_FAILURE;
		}

		if (get_cache_path(filepath, cache_path, sizeof(cache_path)) == 0) {
			struct stat st;
			if (stat(cache_path, &st) == 0) {
				have_cache = true;
			}
		}
	}

	if (stdin_mode && verbose) {
		printf("Running in stdin-only dedup mode\n");
	}

	// initialize or load cached bloom filter
	if (stdin_mode || no_cache) {
		// stdin/no cach mode: create filter with stack size of 0 (infinite)
		// TODO consider defaulting to a larger initial_size
		if (bloom_init(&bf, initial_size, 0.0001f, 0) != BF_SUCCESS) {
			fprintf(stderr, "Failed to initialize Bloom filter\n");
			return EXIT_FAILURE;
		}
	} else {
		if (have_cache && !force_rebuild) {
			if (bloom_load(&bf, cache_path) != BF_SUCCESS) {
				if (verbose) {
					fprintf(stderr, "Failed to load cached filter. Rebuilding...\n");
				}
				have_cache = false;
			}
		}

		if (!have_cache || force_rebuild) {
			size_t expected = is_large_file(filepath) ? count_lines(filepath) * 2 : initial_size;

			if (bloom_init(&bf, expected, 0.0001f, max_stacks) != BF_SUCCESS) {
				fprintf(stderr, "Failed to initialize Bloom filter\n");
				return EXIT_FAILURE;
			}

			if (!bloom_populate_from_file(&bf, filepath)) {
				fprintf(stderr, "Failed to populate Bloom filter from file %s: %s\n",
						filepath, strerror(errno));
				return EXIT_FAILURE;
			}
		}
	}

	// read from stdin, lookup line in filter, add if not seen yet
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	while ((read = getline(&line, &len, stdin)) != -1) {
		if (read > 0 && line[read - 1] == '\n') {
			line[read - 1] = '\0';  // strip newline
		}

		if (!bloom_lookup_or_add_string(&bf, line)) {
			if (verbose) {
				fprintf(stderr, "NEW: %s\n", line);
			}
			fprintf(out, "%s\n", line);
		}

		if (bf.needs_rebuild) {
			if (verbose) {
				fprintf(stderr, "Rebuilding Bloom filter...\n");
			}

			bloomfilter new_bf;
			size_t new_expected = bf.expected * bf.max_stacks * 2;

			if (bloom_init(&new_bf, new_expected, bf.accuracy, max_stacks) != BF_SUCCESS) {
				fprintf(stderr, "error: failed to allocate new filter\n");
				return EXIT_FAILURE;
			}

			if (!bloom_populate_from_file(&new_bf, filepath)) {
				fprintf(stderr, "Failed to re-populate new filter\n");
				return EXIT_FAILURE;
			}

			update_stat_metadata(&new_bf, filepath);
			bloom_destroy(&bf);
			bf = new_bf;
		}
	}

	free(line);

	// save filter for caching purposes, cleanup
	if (!stdin_mode && !no_cache) {
        fflush(out);
		fsync(fileno(out));
		update_stat_metadata(&bf, filepath);

		if (bloom_save(&bf, cache_path) != BF_SUCCESS) {
			fprintf(stderr, "Failed to save cache filter to %s: %s\n",
					cache_path, strerror(errno));
		} else if (verbose) {
			fprintf(stderr, "Saved Bloom filter cache: %s\n", cache_path);
		}
	}

	fclose(out);
	bloom_destroy(&bf);

	return EXIT_SUCCESS;
}
