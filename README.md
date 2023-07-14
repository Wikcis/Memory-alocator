# Memory Allocator

The Memory Allocator project is a C program that emulates the behavior of standard C library functions, namely malloc, calloc, free, and realloc. These functions play a vital role in dynamically allocating memory in C programs. By replicating their behavior, the program aims to provide users with a better understanding of how these functions operate and how they can be effectively utilized in their own code.

## Features

- Emulates the functionality of standard C library functions: malloc, calloc, free, and realloc.
- Provides a simulated environment for dynamic memory allocation in C programs.
- Includes additional utility functions to monitor the state and integrity of the heap area.
- Allows users to reset the heap to its initial state.
- Offers the capability to request heap region size expansion from the system.
- Incorporates debug features such as "fences" to detect memory corruption or other memory management errors.
