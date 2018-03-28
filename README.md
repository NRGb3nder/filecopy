# filecopy
OS and System Programming, lab #6

Write a program to copy large (> 2 GB) files with few processes simultaneously. Input and output file names are specified by user, as well as N, a number of processes. During the program execution file is divided into N parts, each one is associated with a single process. Use semaphores to solve a mutual exclusion problem of simultaneous output.

Command format:
```
<program> <file_to_copy> <destination> <number_of_processes>
```
