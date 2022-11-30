#include <stdio.h>
#include <ctype.h>

static char rfc3986[256] = {0};

void
url_enc_init()
{
    int i;

    for (i = 0; i < 256; i++)
        rfc3986[i] = isalnum(i) || i == '~' || i == '-' || i == '.' || i == '_' ? i : 0;
}

char*
url_encode(const char *s, char *enc)
{
    char *ret = enc;

    for (; *s; ++s){
        if (rfc3986[*(unsigned char*)s])
            sprintf(enc, "%c", rfc3986[*(unsigned char*)s]);
        else
            sprintf(enc, "%%%02X", *(unsigned char*)s);
        while (*++enc);
    }

    return ret;
}
