#include "../include/cli.h"

#include "../include/input_utils.h"
#include "../include/llm_client.h"
#include "../include/style.h"
#include "../include/ui_frame.h"
#include "../include/ui_icons.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_prompt(PromptStore *store, const char *data_file) {
    char name[128];
    char tags[256];
    char content[4096];
    read_line("Prompt name: ", name, sizeof(name));
    read_line("Tags (comma-separated): ", tags, sizeof(tags));
    read_line("Prompt content: ", content, sizeof(content));

    if (prompt_store_add(store, name, tags, content)) {
        if (prompt_store_save(store, data_file)) {
            printf(STYLE_GREEN "Saved.\n" STYLE_RESET);
        } else {
            printf(STYLE_RED "Failed to write file.\n" STYLE_RESET);
        }
    } else {
        printf(STYLE_RED "Save failed: duplicate name or out of memory.\n" STYLE_RESET);
    }
}

static void search_prompt(const PromptStore *store) {
    char keyword[128];
    char tag[64];
    read_line("Keyword (name/content, optional): ", keyword, sizeof(keyword));
    read_line("Tag filter (optional): ", tag, sizeof(tag));
    prompt_store_search(store, keyword, tag);
}

static void execute_prompt(PromptStore *store) {
    if (store->count == 0) {
        printf(STYLE_YELLOW "No prompt to execute.\n" STYLE_RESET);
        return;
    }
    prompt_store_list(store);
    char idx_buf[32];
    read_line("Prompt index to execute: ", idx_buf, sizeof(idx_buf));
    size_t idx = (size_t)strtoul(idx_buf, NULL, 10);
    const Prompt *p = prompt_store_get(store, idx);
    if (!p) {
        printf(STYLE_RED "Invalid index.\n" STYLE_RESET);
        return;
    }
    prompt_store_delete(store, idx);
    char user_input[1024];
    read_line("Extra input (optional): ", user_input, sizeof(user_input));
    printf(STYLE_YELLOW "Calling LLM...\n" STYLE_RESET);
    char *resp = llm_execute_prompt(p->content, user_input);
    if (!resp) {
        printf(STYLE_RED "Call failed: out of memory or network error.\n" STYLE_RESET);
        return;
    }
    printf(STYLE_GREEN "Model output:\n%s\n" STYLE_RESET, resp);
    free(resp);
}

static void delete_prompt(PromptStore *store, const char *data_file) {
    if (store->count == 0) {
        printf(STYLE_YELLOW "No prompt to delete.\n" STYLE_RESET);
        return;
    }
    prompt_store_list(store);
    char idx_buf[32];
    read_line("Prompt index to delete: ", idx_buf, sizeof(idx_buf));
    size_t idx = (size_t)strtoul(idx_buf, NULL, 10);
    if (!prompt_store_delete(store, idx)) {
        printf(STYLE_RED "Delete failed: invalid index.\n" STYLE_RESET);
        return;
    }
    if (!prompt_store_save(store, data_file)) {
        printf(STYLE_RED "Delete succeeded but save failed.\n" STYLE_RESET);
        return;
    }
    printf(STYLE_GREEN "Deleted.\n" STYLE_RESET);
}

static void print_menu(void) {
    printf("\n");
    ui_print_title("Prompt Operations");
    char line1[64];
    char line2[64];
    char line3[64];
    char line4[64];
    char line5[64];
    snprintf(line1, sizeof(line1), "%s Save prompt", ui_icon_prompt());
    snprintf(line2, sizeof(line2), "%s List prompts", ui_icon_tag());
    snprintf(line3, sizeof(line3), "%s Search prompts", ui_icon_search());
    snprintf(line4, sizeof(line4), "%s Execute via LLM API", ui_icon_run());
    snprintf(line5, sizeof(line5), "%s Delete prompt", ui_icon_delete());
    ui_print_menu_item("1)", line1);
    ui_print_menu_item("2)", line2);
    ui_print_menu_item("3)", line3);
    ui_print_menu_item("4)", line4);
    ui_print_menu_item("5)", line5);
    ui_print_menu_item("0)", "Exit");
    ui_print_separator();
    ui_print_hint("Tip: use comma-separated tags for better filtering.");
}

void run_cli(PromptStore *store, const char *data_file) {
    for (;;) {
        print_header();
        print_menu();
        char input[16];
        read_line("Select: ", input, sizeof(input));

        if (strcmp(input, "1") == 0) {
            add_prompt(store, data_file);
        } else if (strcmp(input, "2") == 0) {
            prompt_store_list(store);
        } else if (strcmp(input, "3") == 0) {
            search_prompt(store);
        } else if (strcmp(input, "4") == 0) {
            execute_prompt(store);
        } else if (strcmp(input, "5") == 0) {
            delete_prompt(store, data_file);
        } else if (strcmp(input, "0") == 0) {
            break;
        } else {
            printf(STYLE_RED "Invalid input.\n" STYLE_RESET);
        }
    }
}
