#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SESSION_MAX_USERS 32

struct session_user {
    char name[32];
    char full_name[64];
    uint32_t uid;
};

int session_users(struct session_user *out, int max);
bool session_live(void);
void session_host_name(char *out, int cap);
bool session_authenticate(const char *name, const char *password, const char **error);
int session_start(const char *name, char *error, int cap);
bool session_finished(int pid);
void session_power(bool reboot);
