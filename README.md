# memorandum

Embedded, in-memory database.

Being written for the needs of the [kbDaw](https://github.com/mhhollomon/kbdaw)
project.

Requires C++20.

MIT license.

## Overview

The idea is that - unlike most databases - the rows are directly stored as C++
structures (or classes). Doing so would keep from having to have code that continuously
marshalled and unmarshalled the separate fields into the database.

The drawback, is of course, that joins and projections are not possible.

So, if you want to call this a key-value store rather than a database, that would
make sense.


## Concurrency

This is not concurrency safe without additional locking.
The  methods can be called without problems from different threads. However,
two threads may not call methods at the same time - unless they are both reads.

Caveat Scriptor

## Interface

See [DB API](docs/api.md)


## Using in a project

### Direct copy

This is a header only project. All that would be needed is to copy the contents
of the `src` directory along with its subdirectories to some suitable place
and point your compiler there as an include  directory.

### using CMake and CPM

An easier way, if you use cmake, is to use [CPM](https://github.com/cpm-cmake/CPM.cmake).

Something like the following :

```cmake
include(CPM)

...

CPMAddPackage("gh:mhhollomon/memorandum@0.2.0")

target_link_libraries(my_project PRIVATE memorandum)
```
You can control the creation of the tests by setting
`MEMORANDUM_BUILD_TESTS`. it is on by default.


## Technology
- [Catch2](https://github.com/catchorg/Catch2) for testing framework.
- [CPM](https://github.com/cpm-cmake/CPM.cmake) for managing dependencies.