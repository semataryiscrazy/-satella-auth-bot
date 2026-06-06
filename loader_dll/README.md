# Satella Loader DLL

Esta é a versão DLL do Satella Loader, convertida do executável original.

## Estrutura

- `LoaderDLL.cpp` - Código-fonte principal da DLL
- `LoaderDLL.h` - Header com as funções exportadas
- `LoaderDLL.vcxproj` - Arquivo de projeto Visual Studio

## Funções Exportadas

### StartLoader()
Inicia o loader. Pode ser chamada de qualquer aplicação que carregue a DLL.

```cpp
extern "C" __declspec(dllexport) void StartLoader();
```

### StopLoader()
Para o loader e libera recursos.

```cpp
extern "C" __declspec(dllexport) void StopLoader();
```

## Como Compilar

1. Abra o Visual Studio
2. Crie um novo projeto "Dynamic Library (DLL)" em C++
3. Copie os arquivos para o projeto
4. Configure para x64 Release
5. Compile (Build > Build Solution)

A DLL compilada será gerada em `x64\Release\LoaderDLL.dll`

## Como Usar

```cpp
#include "LoaderDLL.h"

int main() {
    // Carrega a DLL
    HMODULE hDll = LoadLibraryW(L"LoaderDLL.dll");
    
    if (hDll) {
        // Obtém a função
        typedef void (*StartLoaderFunc)();
        StartLoaderFunc StartLoader = (StartLoaderFunc)GetProcAddress(hDll, "StartLoader");
        
        if (StartLoader) {
            StartLoader();
        }
        
        FreeLibrary(hDll);
    }
    
    return 0;
}
```

## Notas

- A DLL foi criada como um wrapper básico do loader original
- Para funcionalidade completa, integre o código do Loader.cpp original
- Certifique-se de incluir todas as dependências necessárias (keyauth, etc.)
