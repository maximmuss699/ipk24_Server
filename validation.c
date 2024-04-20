#include "validation.h"
#include <ctype.h>
#include <string.h>


bool Check_username(const char* username) {
    size_t length = strlen(username);
    if (length > MAX_USERNAME_LENGTH)
        return false;
    for (size_t i = 0; i < length; i++) {
        if (!isalnum(username[i]) && username[i] != '-')
            return false;
    }
    return true;
}

bool Check_secret(const char* secret) {
    size_t length = strlen(secret);
    if (length > MAX_SECRET_LENGTH)
        return false;
    for (size_t i = 0; i < length; i++) {
        if (!isalnum(secret[i]) && secret[i] != '-')
            return false;
    }
    return true;
}

bool Check_Displayname(const char* displayName) {
    size_t length = strlen(displayName);
    if (length > MAX_DISPLAY_NAME_LENGTH) return false;
    for (size_t i = 0; i < length; i++) {
        if (!isprint(displayName[i]) || displayName[i] < 0x21 || displayName[i] > 0x7E)
            return false;
    }
    return true;
}
