#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>

size_t wchar_to_utf8(const wchar_t *source, char *destination,
                     size_t destination_size);
char *trimwhitespace(char *str);

#endif // !HELPERS_H
