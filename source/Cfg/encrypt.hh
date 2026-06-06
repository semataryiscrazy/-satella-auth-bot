#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cctype>

// Declarações de funções
char* remove_spaces(const char* str);
char* random_string(int length);
char* obfuscate_char(const char* texto);
char* restore_char(const char* texto);
char* string_to_hex(const char* input);
char* hex_to_string(const char* input);
char* obfuscate_string(const char* input);
char* restore_string(const char* obfuscated);
char* encrypt(const char* input);
char* decrypt(const char* input);
char* confuse_encrypt(const char* input);
char* confuse_decrypt(const char* encrypted);
char* rs(const char* input);
bool constant_time_compare(const char* str1, const char* str2, size_t len);
size_t str_length(const char* str);
int stringToInt(const std::string& str);
float stringToFloat(const std::string& str);
uintptr_t string2Offset(const char* c);
