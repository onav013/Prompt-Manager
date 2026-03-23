#ifndef PROMPT_STORE_H
#define PROMPT_STORE_H

#include <stddef.h>

typedef struct {
    char *name;
    char **tags;
    size_t tag_count;
    char *content;
} Prompt;

typedef struct {
    Prompt *items;
    size_t count;
    size_t capacity;
} PromptStore;

void prompt_store_init(PromptStore *store);
void prompt_store_free(PromptStore *store);

int prompt_store_add(PromptStore *store, const char *name, const char *tags_csv, const char *content);
int prompt_store_delete(PromptStore *store, size_t index);

int prompt_store_load(PromptStore *store, const char *path);
int prompt_store_save(const PromptStore *store, const char *path);

void prompt_store_list(const PromptStore *store);
void prompt_store_search(const PromptStore *store, const char *keyword, const char *tag_filter);

const Prompt *prompt_store_get(const PromptStore *store, size_t index);

#endif
