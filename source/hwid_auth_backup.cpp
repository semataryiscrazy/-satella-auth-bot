// HWID Auth Backup — reativar quando quiser validacao HWID
// Copie o bloco abaixo no InitIdow() em Main.cpp

/*
    // Load HWID-based auth from loader
    wchar_t cp[MAX_PATH]; GetTempPathW(MAX_PATH, cp);
    wcscat_s(cp, MAX_PATH, L"Satella.cred");
    HANDLE cf = CreateFileW(cp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (cf != INVALID_HANDLE_VALUE) {
        wchar_t bf[512] = {0}; DWORD rb; ReadFile(cf, bf, 510, &rb, NULL); CloseHandle(cf);
        wchar_t* sp = wcschr(bf, L'|');
        if (sp) { *sp = 0;
            WideCharToMultiByte(CP_UTF8, 0, bf, -1, Auth.Usuario, 256, NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, sp + 1, -1, Auth.Senha, 256, NULL, NULL); }
        // HWID auto-auth - loader already validated
        CurrentWindow = 1; CurrentTab = 2;
        Auth.Autenticado = true;
    }
*/

// Copie o bloco abaixo no botao "Connect" do login em Main.cpp

/*
                                // Only allow login if credentials match the loader's HWID-bound auth file
                                wchar_t cp2[MAX_PATH]; GetTempPathW(MAX_PATH, cp2);
                                wcscat_s(cp2, MAX_PATH, L"Satella.cred");
                                bool valid = false;
                                HANDLE cf2 = CreateFileW(cp2, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                                if (cf2 != INVALID_HANDLE_VALUE) {
                                    wchar_t bf2[512]={0}; DWORD rb2; ReadFile(cf2, bf2, 510, &rb2, NULL); CloseHandle(cf2);
                                    wchar_t* sp2 = wcschr(bf2, L'|');
                                    if (sp2) { *sp2 = 0;
                                        char u2[256]="", p2[256]="";
                                        WideCharToMultiByte(CP_UTF8, 0, bf2, -1, u2, 256, NULL, NULL);
                                        WideCharToMultiByte(CP_UTF8, 0, sp2+1, -1, p2, 256, NULL, NULL);
                                        valid = (strcmp(Auth.Usuario, u2) == 0 && strcmp(Auth.Senha, p2) == 0)
                                            && u2[0] != 0 && p2[0] != 0; }
                                }
                                if (valid) {
                                    CurrentWindow = 1; CurrentTab = 2;
                                    Auth.Autenticado = true;
                                    NotificationManager::AdicionarNotificacao("Bem-Vindo,  " + std::string(Auth.Usuario) + "!");
                                    std::thread([]() { DiscordRPC->Initialize(); DiscordRPC->Update(Auth.Usuario, 999); }).detach();
                                    std::thread(NetworkInit).detach();
                                    std::thread([]() { Sleep(2000); LoadLibraryAndHook(); }).detach();
                                } else {
                                    NotificationManager::AdicionarNotificacao("Loader HWID auth required");
                                }
*/
