#include "../include/prompt_store.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, len + 1);
    return out;
}

static int ensure_capacity(PromptStore *store, size_t need) {
    if (store->capacity >= need) {
        return 1;
    }
    size_t next = store->capacity == 0 ? 8 : store->capacity * 2;
    while (next < need) {
        next *= 2;
    }
    Prompt *grown = (Prompt *)realloc(store->items, next * sizeof(Prompt));
    if (!grown) {
        return 0;
    }
    store->items = grown;
    store->capacity = next;
    return 1;
}

static void free_prompt(Prompt *p) {
    if (!p) {
        return;
    }
    free(p->name);
    free(p->content);
    if (p->tags) {
        for (size_t i = 0; i < p->tag_count; ++i) {
            free(p->tags[i]);
        }
        free(p->tags);
    }
    p->name = NULL;
    p->content = NULL;
    p->tags = NULL;
    p->tag_count = 0;
}

static int split_tags(const char *csv, char ***out_tags, size_t *out_count) {
    *out_tags = NULL;
    *out_count = 0;
    if (!csv || !*csv) {
        return 1;
    }

    char *work = dup_str(csv);
    if (!work) {
        return 0;
    }

    size_t cap = 4;
    size_t count = 0;
    char **tags = (char **)malloc(cap * sizeof(char *));
    if (!tags) {
        free(work);
        return 0;
    }

    char *token = strtok(work, ",");
    while (token) {
        while (isspace((unsigned char)*token)) {
            token++;
        }
        size_t n = strlen(token);
        while (n > 0 && isspace((unsigned char)token[n - 1])) {
            token[--n] = '\0';
        }
        if (n > 0) {
            if (count == cap) {
                cap *= 2;
                char **grown = (char **)realloc(tags, cap * sizeof(char *));
                if (!grown) {
                    for (size_t i = 0; i < count; ++i) {
                        free(tags[i]);
                    }
                    free(tags);
                    free(work);
                    return 0;
                }
                tags = grown;
            }
            tags[count] = dup_str(token);
            if (!tags[count]) {
                for (size_t i = 0; i < count; ++i) {
                    free(tags[i]);
                }
                free(tags);
                free(work);
                return 0;
            }
            count++;
        }
        token = strtok(NULL, ",");
    }

    free(work);
    *out_tags = tags;
    *out_count = count;
    return 1;
}

static int contains_case_insensitive(const char *text, const char *needle) {
    if (!needle || !*needle) {
        return 1;
    }
    size_t n = strlen(needle);
    for (const char *p = text; *p; ++p) {
        size_t i = 0;
        while (i < n && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == n) {
            return 1;
        }
    }
    return 0;
}

static int prompt_has_tag(const Prompt *p, const char *tag) {
    if (!tag || !*tag) {
        return 1;
    }
    for (size_t i = 0; i < p->tag_count; ++i) {
        if (contains_case_insensitive(p->tags[i], tag)) {
            return 1;
        }
    }
    return 0;
}

static char *escape_field(const char *src) {
    size_t len = strlen(src);
    size_t extra = 0;
    for (size_t i = 0; i < len; ++i) {
        if (src[i] == '\\' || src[i] == '|' || src[i] == '\n' || src[i] == '\r') {
            extra++;
        }
    }
    char *out = (char *)malloc(len + extra + 1);
    if (!out) {
        return NULL;
    }
    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        if (src[i] == '\\' || src[i] == '|') {
            out[j++] = '\\';
            out[j++] = src[i];
        } else if (src[i] == '\n') {
            out[j++] = '\\';
            out[j++] = 'n';
        } else if (src[i] == '\r') {
            out[j++] = '\\';
            out[j++] = 'r';
        } else {
            out[j++] = src[i];
        }
    }
    out[j] = '\0';
    return out;
}

static char *unescape_field(const char *src) {
    size_t len = strlen(src);
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        if (src[i] == '\\' && i + 1 < len) {
            char n = src[++i];
            if (n == 'n') {
                out[j++] = '\n';
            } else if (n == 'r') {
                out[j++] = '\r';
            } else {
                out[j++] = n;
            }
        } else {
            out[j++] = src[i];
        }
    }
    out[j] = '\0';
    return out;
}

static char *join_tags(const Prompt *p) {
    if (p->tag_count == 0) {
        return dup_str("");
    }
    size_t total = 0;
    for (size_t i = 0; i < p->tag_count; ++i) {
        total += strlen(p->tags[i]) + 1;
    }
    char *out = (char *)malloc(total + 1);
    if (!out) {
        return NULL;
    }
    out[0] = '\0';
    for (size_t i = 0; i < p->tag_count; ++i) {
        strcat(out, p->tags[i]);
        if (i + 1 < p->tag_count) {
            strcat(out, ",");
        }
    }
    return out;
}

static int split_record_fields(char *line, char **f1, char **f2, char **f3) {
    char *fields[3] = {line, NULL, NULL};
    int idx = 0;
    int escaped = 0;
    for (char *p = line; *p; ++p) {
        if (!escaped && *p == '\\') {
            escaped = 1;
            continue;
        }
        if (!escaped && *p == '|') {
            *p = '\0';
            idx++;
            if (idx >= 3) {
                return 0;
            }
            fields[idx] = p + 1;
            continue;
        }
        escaped = 0;
    }
    if (idx != 2 || !fields[0] || !fields[1] || !fields[2]) {
        return 0;
    }
    *f1 = fields[0];
    *f2 = fields[1];
    *f3 = fields[2];
    return 1;
}

void prompt_store_init(PromptStore *store) {
    store->items = NULL;
    store->count = 0;
    store->capacity = 0;
}

void prompt_store_free(PromptStore *store) {
    if (!store) {
        return;
    }
    for (size_t i = 0; i < store->count; ++i) {
        free_prompt(&store->items[i]);
    }
    free(store->items);
    store->items = NULL;
    store->count = 0;
    store->capacity = 0;
}

int prompt_store_add(PromptStore *store, const char *name, const char *tags_csv, const char *content) {
    if (!name || !*name || !content) {
        return 0;
    }
    for (size_t i = 0; i < store->count; ++i) {
        if (strcmp(store->items[i].name, name) == 0) {
            return 0;
        }
    }
    if (!ensure_capacity(store, store->count + 1)) {
        return 0;
    }

    Prompt p;
    p.name = dup_str(name);
    p.content = dup_str(content);
    p.tags = NULL;
    p.tag_count = 0;
    if (!p.name || !p.content) {
        free(p.name);
        free(p.content);
        return 0;
    }
    if (!split_tags(tags_csv, &p.tags, &p.tag_count)) {
        free(p.name);
        free(p.content);
        return 0;
    }

    store->items[store->count++] = p;
    return 1;
}

int prompt_store_delete(PromptStore *store, size_t index) {
    if (index >= store->count) {
        return 0;
    }
    free_prompt(&store->items[index]);
    for (size_t i = index; i + 1 < store->count; ++i) {
        store->items[i] = store->items[i + 1];
    }
    store->count--;
    return 1;
}

const Prompt *prompt_store_get(const PromptStore *store, size_t index) {
    if (index >= store->count) {
        return NULL;
    }
    return &store->items[index];
}

int prompt_store_load(PromptStore *store, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return 1;
    }

    char line[8192];
    while (fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }

        char *raw_name = NULL;
        char *raw_tags = NULL;
        char *raw_content = NULL;
        if (!split_record_fields(line, &raw_name, &raw_tags, &raw_content)) {
            continue;
        }

        char *name = unescape_field(raw_name);
        char *tags = unescape_field(raw_tags);
        char *content = unescape_field(raw_content);
        if (!name || !tags || !content) {
            free(name);
            free(tags);
            free(content);
            fclose(fp);
            return 0;
        }
        int ok = prompt_store_add(store, name, tags, content);
        free(name);
        free(tags);
        free(content);
        if (!ok) {
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return 1;
}

int prompt_store_save(const PromptStore *store, const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }

    for (size_t i = 0; i < store->count; ++i) {
        const Prompt *p = &store->items[i];
        char *tag_csv = join_tags(p);
        char *name_e = escape_field(p->name);
        char *tags_e = tag_csv ? escape_field(tag_csv) : NULL;
        char *content_e = escape_field(p->content);
        free(tag_csv);
        if (!name_e || !tags_e || !content_e) {
            free(name_e);
            free(tags_e);
            free(content_e);
            fclose(fp);
            return 0;
        }
        fprintf(fp, "%s|%s|%s\n", name_e, tags_e, content_e);
        free(name_e);
        free(tags_e);
        free(content_e);
    }

    fclose(fp);
    return 1;
}

void prompt_store_list(const PromptStore *store) {
    if (store->count == 0) {
        printf("No prompts.\n");
        return;
    }
    for (size_t i = 0; i < store->count; ++i) {
        const Prompt *p = &store->items[i];
        printf("[%zu] %s  tags: ", i, p->name);
        if (p->tag_count == 0) {
            printf("(none)");
        } else {
            for (size_t t = 0; t < p->tag_count; ++t) {
                printf("%s%s", p->tags[t], (t + 1 < p->tag_count) ? ", " : "");
            }
        }
        printf("\n");
    }
}

void prompt_store_search(const PromptStore *store, const char *keyword, const char *tag_filter) {
    int found = 0;
    for (size_t i = 0; i < store->count; ++i) {
        const Prompt *p = &store->items[i];
        int hit_keyword = (!keyword || !*keyword) ||
                          contains_case_insensitive(p->name, keyword) ||
                          contains_case_insensitive(p->content, keyword);
        int hit_tag = prompt_has_tag(p, tag_filter);
        if (hit_keyword && hit_tag) {
            found = 1;
            printf("[%zu] %s\n", i, p->name);
        }
    }
    if (!found) {
        printf("No search results.\n");
    }
}
