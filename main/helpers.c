#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

size_t wchar_to_utf8(
    const wchar_t *source,
    char *destination,
    size_t destination_size
) {
    size_t written = 0;

    if (source == NULL ||
        destination == NULL ||
        destination_size == 0) {
        return 0;
    }

    while (*source != L'\0') {
        uint32_t codepoint = (uint32_t)*source++;
        uint8_t encoded[4];
        size_t encoded_length;

        /*
         * Replace invalid Unicode values with U+FFFD.
         *
         * wchar_t is normally 32 bits on ESP-IDF, so surrogate-pair
         * processing generally isn't necessary.
         */
        if (codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            codepoint = 0xfffd;
        }

        if (codepoint <= 0x7f) {
            encoded[0] = (uint8_t)codepoint;
            encoded_length = 1;
        } else if (codepoint <= 0x7ff) {
            encoded[0] = 0xc0 | (codepoint >> 6);
            encoded[1] = 0x80 | (codepoint & 0x3f);
            encoded_length = 2;
        } else if (codepoint <= 0xffff) {
            encoded[0] = 0xe0 | (codepoint >> 12);
            encoded[1] = 0x80 | ((codepoint >> 6) & 0x3f);
            encoded[2] = 0x80 | (codepoint & 0x3f);
            encoded_length = 3;
        } else {
            encoded[0] = 0xf0 | (codepoint >> 18);
            encoded[1] = 0x80 | ((codepoint >> 12) & 0x3f);
            encoded[2] = 0x80 | ((codepoint >> 6) & 0x3f);
            encoded[3] = 0x80 | (codepoint & 0x3f);
            encoded_length = 4;
        }

        /*
         * Reserve one byte for the terminating null.
         */
        if (written + encoded_length >= destination_size) {
            break;
        }

        for (size_t i = 0; i < encoded_length; i++) {
            destination[written++] = (char)encoded[i];
        }
    }

    destination[written] = '\0';
    return written;
}
