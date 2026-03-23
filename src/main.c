#include "../include/cli.h"
#include "../include/prompt_store.h"
#include "../include/style.h"

#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define DATA_FILE "prompts.db"

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    PromptStore store;
    prompt_store_init(&store);

    if (!prompt_store_load(&store, DATA_FILE)) {
        printf(STYLE_RED "Failed to load data. File may be corrupted.\n" STYLE_RESET);
    }

    run_cli(&store, DATA_FILE);

    prompt_store_free(&store);
    return 0;
}
