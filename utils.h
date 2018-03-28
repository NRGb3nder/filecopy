#ifndef FILECOPY_UTILS_H
#define FILECOPY_UTILS_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>

void printerr(const char *module, const char *errmsg, const char *comment);
bool isreg(const char *path);

#endif //FILECOPY_UTILS_H
