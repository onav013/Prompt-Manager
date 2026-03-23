#include "../include/llm_client.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

static char *dup_str_local(const char *s) {
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, len + 1);
    return out;
}

static char *json_escape(const char *s) {
    size_t len = strlen(s);
    size_t extra = 0;
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == '"' || s[i] == '\\' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t') {
            extra++;
        }
    }
    char *out = (char *)malloc(len + extra + 1);
    if (!out) {
        return NULL;
    }
    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = s[i];
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
            out[j++] = c;
        } else if (c == '\n') {
            out[j++] = '\\';
            out[j++] = 'n';
        } else if (c == '\r') {
            out[j++] = '\\';
            out[j++] = 'r';
        } else if (c == '\t') {
            out[j++] = '\\';
            out[j++] = 't';
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    return out;
}

static char *trim_and_unwrap(const char *src) {
    if (!src) {
        return NULL;
    }
    while (*src && isspace((unsigned char)*src)) {
        src++;
    }
    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1])) {
        len--;
    }
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, src, len);
    out[len] = '\0';
    for (;;) {
        size_t n = strlen(out);
        if (n < 2) {
            break;
        }
        char first = out[0];
        char last = out[n - 1];
        int wrapped = (first == '"' && last == '"') ||
                      (first == '\'' && last == '\'') ||
                      (first == '`' && last == '`');
        if (!wrapped) {
            break;
        }
        memmove(out, out + 1, n - 2);
        out[n - 2] = '\0';
        while (*out && isspace((unsigned char)*out)) {
            memmove(out, out + 1, strlen(out));
        }
        n = strlen(out);
        while (n > 0 && isspace((unsigned char)out[n - 1])) {
            out[--n] = '\0';
        }
    }
    return out;
}

static char *ensure_chat_endpoint(const char *api_url) {
    if (!api_url || !*api_url) {
        return NULL;
    }
    const char *scheme = strstr(api_url, "://");
    const char *host_start = scheme ? scheme + 3 : api_url;
    const char *slash = strchr(host_start, '/');
    if (!slash || strcmp(slash, "/") == 0) {
        const char *suffix = "/v1/chat/completions";
        size_t base_len = strlen(api_url);
        int has_trailing = base_len > 0 && api_url[base_len - 1] == '/';
        size_t need = base_len + strlen(suffix) + 2;
        char *out = (char *)malloc(need);
        if (!out) {
            return NULL;
        }
        if (has_trailing) {
            snprintf(out, need, "%s%s", api_url, suffix + 1);
        } else {
            snprintf(out, need, "%s%s", api_url, suffix);
        }
        return out;
    }
    return dup_str_local(api_url);
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static size_t append_utf8(char *out, size_t len, unsigned cp) {
    if (cp <= 0x7F) {
        out[len++] = (char)cp;
    } else if (cp <= 0x7FF) {
        out[len++] = (char)(0xC0 | (cp >> 6));
        out[len++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out[len++] = (char)(0xE0 | (cp >> 12));
        out[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[len++] = (char)(0x80 | (cp & 0x3F));
    } else {
        out[len++] = (char)(0xF0 | (cp >> 18));
        out[len++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[len++] = (char)(0x80 | (cp & 0x3F));
    }
    return len;
}

static char *extract_json_string_after_key(const char *json, const char *key_name) {
    char key_pat[64];
    snprintf(key_pat, sizeof(key_pat), "\"%s\"", key_name);
    const char *p = json;
    while ((p = strstr(p, key_pat)) != NULL) {
        p += strlen(key_pat);
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p != ':') {
            continue;
        }
        p++;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p != '"') {
            continue;
        }
        p++;

        size_t cap = 256;
        size_t len = 0;
        char *out = (char *)malloc(cap);
        if (!out) {
            return NULL;
        }

        int esc = 0;
        while (*p) {
            char c = *p++;
            if (!esc && c == '"') {
                break;
            }
            if (!esc && c == '\\') {
                esc = 1;
                continue;
            }
            if (esc) {
                if (c == 'n') c = '\n';
                else if (c == 'r') c = '\r';
                else if (c == 't') c = '\t';
                else if (c == 'u') {
                    if (p[0] && p[1] && p[2] && p[3]) {
                        int h0 = hex_value(p[0]);
                        int h1 = hex_value(p[1]);
                        int h2 = hex_value(p[2]);
                        int h3 = hex_value(p[3]);
                        if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
                            unsigned cp = (unsigned)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
                            p += 4;
                            while (len + 5 > cap) {
                                cap *= 2;
                                char *grown = (char *)realloc(out, cap);
                                if (!grown) {
                                    free(out);
                                    return NULL;
                                }
                                out = grown;
                            }
                            len = append_utf8(out, len, cp);
                            esc = 0;
                            continue;
                        }
                    }
                    c = 'u';
                }
                esc = 0;
            }
            if (len + 2 > cap) {
                cap *= 2;
                char *grown = (char *)realloc(out, cap);
                if (!grown) {
                    free(out);
                    return NULL;
                }
                out = grown;
            }
            out[len++] = c;
        }
        out[len] = '\0';
        return out;
    }
    return NULL;
}

static char *extract_content_field(const char *json) {
    char *content = extract_json_string_after_key(json, "content");
    if (content && *content) {
        return content;
    }
    free(content);

    char *text = extract_json_string_after_key(json, "text");
    if (text && *text) {
        return text;
    }
    free(text);

    char *err = extract_json_string_after_key(json, "message");
    if (err && *err) {
        size_t n = strlen(err) + 32;
        char *out = (char *)malloc(n);
        if (!out) {
            free(err);
            return NULL;
        }
        snprintf(out, n, "API error: %s", err);
        free(err);
        return out;
    }
    free(err);

    size_t raw_len = strlen(json);
    if (raw_len > 280) {
        raw_len = 280;
    }
    char *out = (char *)malloc(raw_len + 48);
    if (!out) {
        return NULL;
    }
    snprintf(out, raw_len + 48, "Cannot parse model response. Raw: %.*s", (int)raw_len, json);
    return out;
}

#ifdef _WIN32
static wchar_t *utf8_to_wide(const char *src) {
    int need = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    if (need <= 0) {
        return NULL;
    }
    wchar_t *out = (wchar_t *)malloc((size_t)need * sizeof(wchar_t));
    if (!out) {
        return NULL;
    }
    if (!MultiByteToWideChar(CP_UTF8, 0, src, -1, out, need)) {
        free(out);
        return NULL;
    }
    return out;
}

static int split_url(const char *url, char **host, int *port, int *secure, char **path) {
    const char *scheme_https = "https://";
    const char *scheme_http = "http://";
    const char *p = NULL;
    *secure = 1;
    *port = 443;

    if (strncmp(url, scheme_https, strlen(scheme_https)) == 0) {
        p = url + strlen(scheme_https);
        *secure = 1;
        *port = 443;
    } else if (strncmp(url, scheme_http, strlen(scheme_http)) == 0) {
        p = url + strlen(scheme_http);
        *secure = 0;
        *port = 80;
    } else {
        return 0;
    }

    const char *slash = strchr(p, '/');
    const char *host_end = slash ? slash : (p + strlen(p));
    const char *colon = NULL;
    const char *it = p;
    for (; it < host_end; ++it) {
        if (*it == ':') {
            colon = it;
            break;
        }
    }

    size_t host_len = (size_t)((colon ? colon : host_end) - p);
    *host = (char *)malloc(host_len + 1);
    if (!*host) {
        return 0;
    }
    memcpy(*host, p, host_len);
    (*host)[host_len] = '\0';

    if (colon) {
        *port = atoi(colon + 1);
    }
    if (slash) {
        *path = dup_str_local(slash);
    } else {
        *path = dup_str_local("/");
    }
    if (!*path) {
        free(*host);
        return 0;
    }
    return 1;
}
#endif

char *llm_execute_prompt(const char *prompt_text, const char *user_input) {
    char *api_url_raw = trim_and_unwrap(getenv("PM_API_URL"));
    char *api_key = trim_and_unwrap(getenv("PM_API_KEY"));
    char *model = trim_and_unwrap(getenv("PM_MODEL"));
    char *api_url = api_url_raw ? ensure_chat_endpoint(api_url_raw) : NULL;
    free(api_url_raw);

    if (!api_url || !*api_url || !api_key || !*api_key || !model || !*model) {
        free(api_url);
        free(api_key);
        free(model);
        return dup_str_local("Please set PM_API_URL / PM_API_KEY / PM_MODEL.");
    }

    size_t in_len = user_input ? strlen(user_input) : 0;
    size_t merged_len = strlen(prompt_text) + in_len + 32;
    char *merged = (char *)malloc(merged_len);
    if (!merged) {
        free(api_url);
        free(api_key);
        free(model);
        return NULL;
    }
    if (in_len > 0) {
        snprintf(merged, merged_len, "%s\n\nExtra user input:\n%s", prompt_text, user_input);
    } else {
        snprintf(merged, merged_len, "%s", prompt_text);
    }

    char *esc_content = json_escape(merged);
    char *esc_model = json_escape(model);
    free(merged);
    if (!esc_content || !esc_model) {
        free(esc_content);
        free(esc_model);
        free(api_url);
        free(api_key);
        free(model);
        return NULL;
    }

    size_t body_len = strlen(esc_content) + strlen(esc_model) + 256;
    char *body = (char *)malloc(body_len);
    if (!body) {
        free(esc_content);
        free(esc_model);
        free(api_url);
        free(api_key);
        free(model);
        return NULL;
    }
    snprintf(body, body_len,
             "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"temperature\":0.7}",
             esc_model, esc_content);
    free(esc_content);
    free(esc_model);

#ifdef _WIN32
    char *host = NULL;
    char *path = NULL;
    int port = 443;
    int secure = 1;
    if (!split_url(api_url, &host, &port, &secure, &path)) {
        free(body);
        free(api_url);
        free(api_key);
        free(model);
        return dup_str_local("Invalid PM_API_URL format.");
    }

    HINTERNET h_session = WinHttpOpen(L"PromptManager/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h_session) {
        free(host);
        free(path);
        free(body);
        free(api_url);
        free(api_key);
        free(model);
        return dup_str_local("WinHTTP initialization failed.");
    }

    wchar_t *w_host = utf8_to_wide(host);
    wchar_t *w_path = utf8_to_wide(path);
    free(host);
    free(path);
    if (!w_host || !w_path) {
        free(w_host);
        free(w_path);
        WinHttpCloseHandle(h_session);
        free(body);
        free(api_url);
        free(api_key);
        free(model);
        return dup_str_local("URL encoding failed.");
    }

    HINTERNET h_connect = WinHttpConnect(h_session, w_host, (INTERNET_PORT)port, 0);
    if (!h_connect) {
        free(w_host);
        free(w_path);
        WinHttpCloseHandle(h_session);
        free(body);
        free(api_url);
        free(api_key);
        free(model);
        return dup_str_local("Network connect failed.");
    }

    DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET h_request = WinHttpOpenRequest(h_connect, L"POST", w_path, NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    free(w_host);
    free(w_path);
    if (!h_request) {
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        free(body);
        free(api_url);
        free(api_key);
        free(model);
        return dup_str_local("Request initialization failed.");
    }

    size_t header_len = strlen(api_key) + 128;
    char *headers_utf8 = (char *)malloc(header_len);
    if (!headers_utf8) {
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        free(body);
        free(api_url);
        free(api_key);
        free(model);
        return NULL;
    }
    snprintf(headers_utf8, header_len,
             "Content-Type: application/json\r\nAuthorization: Bearer %s\r\n", api_key);
    wchar_t *headers_w = utf8_to_wide(headers_utf8);
    free(headers_utf8);
    if (!headers_w) {
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        free(body);
        free(api_url);
        free(api_key);
        free(model);
        return NULL;
    }

    BOOL ok = WinHttpSendRequest(h_request, headers_w, (DWORD)-1L, body, (DWORD)strlen(body), (DWORD)strlen(body), 0);
    free(headers_w);
    free(body);
    if (!ok || !WinHttpReceiveResponse(h_request, NULL)) {
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        free(api_url);
        free(api_key);
        free(model);
        return dup_str_local("Request failed. Check API URL and key.");
    }

    size_t cap = 4096;
    size_t total = 0;
    char *resp = (char *)malloc(cap);
    if (!resp) {
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        free(api_url);
        free(api_key);
        free(model);
        return NULL;
    }

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(h_request, &avail) || avail == 0) {
            break;
        }
        if (total + avail + 1 > cap) {
            while (total + avail + 1 > cap) {
                cap *= 2;
            }
            char *grown = (char *)realloc(resp, cap);
            if (!grown) {
                free(resp);
                WinHttpCloseHandle(h_request);
                WinHttpCloseHandle(h_connect);
                WinHttpCloseHandle(h_session);
                free(api_url);
                free(api_key);
                free(model);
                return NULL;
            }
            resp = grown;
        }
        DWORD read = 0;
        if (!WinHttpReadData(h_request, resp + total, avail, &read)) {
            free(resp);
            WinHttpCloseHandle(h_request);
            WinHttpCloseHandle(h_connect);
            WinHttpCloseHandle(h_session);
            free(api_url);
            free(api_key);
            free(model);
            return dup_str_local("Failed reading response.");
        }
        total += read;
    }
    resp[total] = '\0';

    WinHttpCloseHandle(h_request);
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);

    char *content = extract_content_field(resp);
    free(resp);
    free(api_url);
    free(api_key);
    free(model);
    return content;
#else
    free(body);
    free(api_url);
    free(api_key);
    free(model);
    return dup_str_local("This build includes only Windows WinHTTP client.");
#endif
}
