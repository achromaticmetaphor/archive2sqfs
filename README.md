archive2sqfs is a tool to convert an archive to a SquashFS image.
archive2sqfs is currently usable, but not yet feature-complete.

What works?
-----------
- directories, regular files, symlinks, sockets, pipes, device files
- fragments
- deduplication
- gzip compression
- zstd compression

What doesn't work?
------------------
- extended attributes
- sparseness
- directory indexes
- lzma, lzo, lz4, and xz compression

Why couldn't I just use mksquashfs?
-----------------------------------
- Maybe you don't have the space to unpack the archive.
- Maybe you want the output to be reproducible.

How do I build it?
------------------
    cmake .
    make
There are optional dependencies, enabled via CMake variables:
- USE_ZSTD=1 enables zstd compression via libzstd.

How do I use it?
----------------
    archive2sqfs [--strip=N] [--compressor=<type>] [--enable-dedup] [--single-thread] outfile [infile]

- The --strip option removes leading directories from archive entries.
- If the infile parameter is omitted, the input archive will be read from stdin.
