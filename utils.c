#include "utils.h"

void printerr(const char *module, const char *errmsg, const char *comment)
{
    fprintf(stderr, "%s: %s ", module, errmsg);
    if (comment) {
        fprintf(stderr, "(%s)", comment);
    }
    fprintf(stderr, "\n");
}

bool isreg(const char *path)
{
    struct stat statbuf;

    if (lstat(path, &statbuf) == -1) {
        return false;
    }

    return S_ISREG(statbuf.st_mode);
}