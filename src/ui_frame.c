#include "../include/ui_frame.h"

#include "../include/style.h"

#include <stdio.h>

void ui_print_title(const char *title) {
    printf(STYLE_CYAN "+--------------------------------------+\n" STYLE_RESET);
    printf(STYLE_CYAN "| %-36s |\n" STYLE_RESET, title);
    printf(STYLE_CYAN "+--------------------------------------+\n" STYLE_RESET);
}

void ui_print_separator(void) {
    printf(STYLE_CYAN "----------------------------------------\n" STYLE_RESET);
}

void ui_print_menu_item(const char *key, const char *label) {
    printf(STYLE_GREEN "%s" STYLE_RESET "  %s\n", key, label);
}

void ui_print_hint(const char *text) {
    printf(STYLE_YELLOW "%s\n" STYLE_RESET, text);
}
