#pragma once

int ka_init();
int ka_login(const char* username, const char* password);
int ka_register(const char* username, const char* password, const char* key);
const char* ka_get_username();
const char* ka_get_error();
int ka_get_days_remaining();
void ka_cleanup();
