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

When working with very large files, it helps to create a large filter
straight away instead of relying on the software to stack
filters. This dramatically reduces collisions in the bloom filters and
yields a more accurate output file:

```
% wc -l rockyou.txt
14344393 rockyou.txt

% cat ~/Downloads/rockyou.txt | new -s 20000000 outfile
```

## Caveat

This uses bloom filters to aid with de-duplication. As such, false
positives are possible, but highly unlikely unless ran with
inappropriate sizing parameters.
