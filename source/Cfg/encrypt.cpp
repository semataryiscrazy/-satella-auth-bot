#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cctype>

#include "encrypt.hh"

#include <algorithm>  // Para std::remove_if
#include <cctype>     // Para std::isspace

char* remove_spaces(const char* str) {
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1); // Allocate memory for the result string
    if (!result) return NULL; // Check if allocation failed

    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        if (!isspace(str[i])) { // Check if the character is not a space
            result[j++] = str[i]; // Copy non-space character to result
        }
    }
    result[j] = '\0'; // Null-terminate the result string
    return result;
}

/*std::string remove_spaces(const std::string& str) {
    std::string result = str;
    result.erase(std::remove_if(result.begin(), result.end(),
        [](unsigned char x) { return std::isspace(x); }), result.end());
    return result;
}*/

char* random_string(int length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char* str = (char*)malloc((length + 1) * sizeof(char));
    if (!str) return NULL;
    srand(static_cast<unsigned int>(time(nullptr)));
    for (int i = 0; i < length; i++) {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    str[length] = '\0';
    return str;
}

char* obfuscate_char(const char* texto) {
    char alfabeto[] = "abcdefghijklmnopqrstuvwxyz";
    char* criptografado = (char*)malloc(strlen(texto) + 1);
    if (!criptografado) return NULL;
    for (size_t i = 0; i < strlen(texto); i++) {
        if (texto[i] >= 'a' && texto[i] <= 'z') {
            criptografado[i] = alfabeto[(texto[i] - 'a' + 3) % 26];
        }
        else {
            criptografado[i] = texto[i];
        }
    }
    criptografado[strlen(texto)] = '\0';
    return criptografado;
}

char* restore_char(const char* texto) {
    char alfabeto[] = "abcdefghijklmnopqrstuvwxyz";
    char* descriptografado = (char*)malloc(strlen(texto) + 1);
    if (!descriptografado) return NULL;
    for (size_t i = 0; i < strlen(texto); i++) {
        if (texto[i] >= 'a' && texto[i] <= 'z') {
            descriptografado[i] = alfabeto[(texto[i] - 'a' - 3 + 26) % 26];
        }
        else {
            descriptografado[i] = texto[i];
        }
    }
    descriptografado[strlen(texto)] = '\0';
    return descriptografado;
}

char* string_to_hex(const char* input) {
    size_t len = strlen(input);
    char* output = (char*)malloc(len * 2 + 1);
    if (!output) return NULL;
    for (size_t i = 0; i < len; ++i) {
        sprintf_s(output + i * 2, 3, "%02X", (unsigned char)input[i]);
    }
    return output;
}

char* hex_to_string(const char* input) {
    size_t len = strlen(input);
    if (len % 2 != 0) return NULL;
    size_t output_len = len / 2;
    char* output = (char*)malloc(output_len + 1);
    if (!output) return NULL;
    for (size_t i = 0; i < output_len; ++i) {
        sscanf_s(input + i * 2, "%2hhX", &output[i]);
    }
    output[output_len] = '\0';
    return output;
}

char* obfuscate_string(const char* input) {
    size_t len = strlen(input);
    char* obfuscated = (char*)malloc(len + 1);
    if (!obfuscated) return NULL;
    for (size_t i = 0; i < len; i++) {
        obfuscated[i] = input[(i + 8) % len];
    }
    obfuscated[len] = '\0';
    return obfuscated;
}

char* restore_string(const char* obfuscated) {
    size_t len = strlen(obfuscated);
    char* original = (char*)malloc(len + 1);
    if (!original) return NULL;
    for (size_t i = 0; i < len; i++) {
        original[i] = obfuscated[(i - 8 + len) % len];
    }
    original[len] = '\0';
    return original;
}

char* encrypt(const char* input) {
    char* TokenKey = string_to_hex(input);
    if (!TokenKey) return NULL;
    char* TempKey = TokenKey;
    TokenKey = obfuscate_string(TokenKey);
    TokenKey = obfuscate_string(TokenKey);
    TokenKey = obfuscate_string(TokenKey);
    TokenKey = obfuscate_string(TokenKey);
    free(TempKey);
    return TokenKey;
}

char* decrypt(const char* input) {
    char* RestoreKey = restore_string(input);
    if (!RestoreKey) return NULL;
    char* TempKey = RestoreKey;
    RestoreKey = restore_string(RestoreKey);
    RestoreKey = restore_string(RestoreKey);
    RestoreKey = restore_string(RestoreKey);
    RestoreKey = hex_to_string(RestoreKey);
    free(TempKey);
    return RestoreKey;
}

char* rs(const char* input) {
    char* String = obfuscate_string(input);
    return String;
}

bool constant_time_compare(const char* str1, const char* str2, size_t len) {
    unsigned char result = 0;
    for (size_t i = 0; i < len; ++i) {
        result |= str1[i] ^ str2[i];
    }
    return result == 0;
}

size_t str_length(const char* str) {
    const char* p = str;
    while (*p) {
        ++p;
    }
    return p - str;
}

#include <iostream>
#include <string>

int stringToInt(const std::string& str) {
    return std::stoi(str);
}

float stringToFloat(const std::string& str) {
    return std::stof(str);
}

uintptr_t string2Offset(const char* c) {
    int base = 16;
    static_assert(sizeof(uintptr_t) == sizeof(unsigned long) || sizeof(uintptr_t) == sizeof(unsigned long long), "");

    if (sizeof(uintptr_t) == sizeof(unsigned long)) {
        return strtoul(c, nullptr, base);
    }
    return strtoull(c, nullptr, base);
}
