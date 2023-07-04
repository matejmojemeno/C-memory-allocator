# C-memory-allocator

A very simple C memory allocator. It is written in C++ becuase it was for a school project, but it functions as the C functions **malloc** and **free**. It is based on the **buddy system**, where the size of each block of allocated memory is a power of 2. This makes the allocation fast (both allocating and freeing memory are O(log(n)) ), and also reduces fragmentation.

The whole thing is currently in main.cpp and is being reworked from what we needed in school to an actual usable project.
