# new - Fast Deduplication with Bloom Filters

`new` adds new lines to a file and deduplicates streams. This has been
useful for me for inserting new passwords into wordlists, bug bounty
recon pipelines, and more.

This should work well on very large files and in memory-constrained
environments.

Inspired by `anew` by tomnomnom: https://github.com/tomnomnom/anew

## Installation

```sh
git clone git@github.com:droberson/new.git
cd new
make
sudo cp new /usr/local/bin
```

## Usage

Insert lines from somefile.txt into outfile only if they do not exist
in outfile:

```sh
cat somefile.txt | new outfile
```

De-duplicate somefile.txt to stdout:

```sh
cat somefile.txt | new
```

## Caveat

This uses bloom filters to aid with de-duplication. As such, false
positives are possible, but highly unlikely unless ran with
inappropriate sizing parameters.
