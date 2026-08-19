# PAK I/O

This library and tool-set provides a modern implementation of the PAK format, with an emphasis on simple, efficient access to archived data. The file itself is an uncompressed compound file, made up of individual files. It is fast to load and parse.

## History

The PAK file format was introduced by id Software for **Quake (1996)** as a simple archive format for packaging a game's assets into a single file. It followed in the tradition of Doom's WAD files, providing a straightforward way to bundle textures, sounds, maps, models, and other game data together rather than storing everything as individual files.

A PAK file consists of a small header, a directory of contained files, and the file data itself. The format is deliberately simple, making it easy to read, write, and inspect without the complexity of a general-purpose archive format.

One of the practical advantages of using a PAK is **fast asset loading**. Instead of opening and searching for potentially thousands of individual files, an application can open a single archive and use its directory to locate the required data. This reduces filesystem operations and makes loading large collections of small assets considerably more efficient, particularly on older storage systems.

## API Overview

- [Class List](annotated.html)
- [Class Hierarchy](hierarchy.html)
- [Namespace List](namespaces.html)
- [File List](files.html)
