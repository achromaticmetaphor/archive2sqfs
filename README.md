archive2sqfs is a tool to convert an archive to a SquashFS image.
archive2sqfs is currently usable, but not yet feature-complete.

What works?
-----------
- directories, regular files, symlinks, sockets, pipes, device files
- fragments
- gzip compression

What doesn't work?
------------------
- extended attributes
- deduplication
- sparseness
- directory indexes
- lzma, lzo, lz4, and xz compression
- older SquashFS versions

Shouldn't you add those?
------------------------
Yes, eventually.

Why couldn't I just use mksquashfs?
-----------------------------------
- Maybe you don't have the space to unpack the archive.
- Maybe you want the output to be reproducible.

How do I build it?
------------------
    cmake .
    make

How do I use it?
----------------
    archive2sqfs [--strip=N] [--compressor=<zlib|none>] outfile [infile]

- The --strip option removes leading directories from archive entries.
- If the infile parameter is omitted, the input archive will be read from stdin.
