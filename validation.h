#ifndef VALIDATION_H
#define VALIDATION_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_USERNAME_LENGTH 20
#define MAX_SECRET_LENGTH 128
#define MAX_DISPLAY_NAME_LENGTH 20

bool Check_username(const char* username);
bool Check_secret(const char* secret);
bool Check_Displayname(const char* displayName);

#endif // VALIDATION_H
