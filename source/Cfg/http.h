#pragma once
#define URLSIZE 1024
#define MAX_BUFFER_SIZE 1024

void xorEncryptDecrypt(char* data, size_t len, const char* key) {
    size_t keyLen = strlen(key);
    for (size_t i = 0; i < len; i++) {
        data[i] ^= key[i % keyLen];
    }
}

std::string http_get(const char* url) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    }
    const char* protocol_end = strstr(url, "://");
    const char* host_start = (protocol_end ? protocol_end + 3 : url);
    const char* path_start = strchr(host_start, '/');

    if (!path_start) {
        path_start = "/";
    }

    size_t host_length = path_start - host_start;
    size_t path_length = strlen(path_start);

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
    }

    auto server = gethostbyname(std::string(host_start, host_length).c_str());
    if (server == NULL) {
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(80);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), AY_OBFUSCATE("GET %s HTTP/1.1\r\nHost: %.*s\r\nConnection: close\r\n\r\n"), path_start, (int)host_length, host_start);

    if (send(sockfd, request, strlen(request), 0) == SOCKET_ERROR) {
    }

    char response[MAX_BUFFER_SIZE];
    int bytesRead;
    while ((bytesRead = recv(sockfd, response, sizeof(response) - 1, 0)) > 0) {
        response[bytesRead] = '\0';
        return response;
    }

    closesocket(sockfd);
    WSACleanup();
    return response;
}

char* GetHostAddrFromUrl(const char* strUrl) {
    char url[URLSIZE] = { 0 };
    strncpy_s(url, strUrl, URLSIZE - 1);

    const char* strAddr = strstr(url, AY_OBFUSCATE("http://"));
    if (strAddr) {
        strAddr += 7;
    }
    else if ((strAddr = strstr(url, AY_OBFUSCATE("https://")))) {
        strAddr += 8;
    }
    else {
        strAddr = url;
    }

    int iLen = strcspn(strAddr, "/");
    char* strHostAddr = (char*)malloc(iLen + 1);
    strncpy_s(strHostAddr, iLen + 1, strAddr, iLen);
    strHostAddr[iLen] = '\0';

    return strHostAddr;
}

char* GetIPFromUrl(const char* strUrl)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    }
    const char* protocol_end = strstr(strUrl, "://");
    const char* host_start = (protocol_end ? protocol_end + 3 : strUrl);
    const char* path_start = strchr(host_start, '/');

    if (!path_start) {
        path_start = "/";
    }

    size_t host_length = path_start - host_start;
    size_t path_length = strlen(path_start);

    struct hostent* he = gethostbyname(std::string(host_start, host_length).c_str());
    
    if (!he) return NULL;

    struct in_addr** addr_list = (struct in_addr**)he->h_addr_list;
    if (addr_list[0]) {
        char* ip = inet_ntoa(*addr_list[0]);
        char* ip_dup = _strdup(ip);
        xorEncryptDecrypt(ip, strlen(ip), AY_OBFUSCATE("???"));
        return ip_dup;
    }
    return NULL;
}
