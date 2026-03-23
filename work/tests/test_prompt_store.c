#include "prompt_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *db = "test_prompts.db";
    PromptStore store;
    prompt_store_init(&store);

    assert(prompt_store_add(&store, "assistant", "coding,tools", "You are coding assistant.") == 1);
    assert(prompt_store_add(&store, "assistant", "dup", "duplicate") == 0);
    assert(prompt_store_add(&store, "writer", "creative", "Write a story.") == 1);
    assert(store.count == 2);

    assert(prompt_store_save(&store, db) == 1);
    prompt_store_free(&store);

    prompt_store_init(&store);
    assert(prompt_store_load(&store, db) == 1);
    assert(store.count == 2);

    {
        const Prompt *p0 = prompt_store_get(&store, 0);
        const Prompt *p1 = prompt_store_get(&store, 1);
        assert(p0 != NULL);
        assert(p1 != NULL);
        assert(strcmp(p0->name, "assistant") == 0);
        assert(strcmp(p1->name, "writer") == 0);
    }

    assert(prompt_store_delete(&store, 0) == 1);
    assert(store.count == 1);
    assert(prompt_store_delete(&store, 9) == 0);

    prompt_store_free(&store);
    remove(db);
    printf("prompt_store_tests passed\n");
    return 0;
}
