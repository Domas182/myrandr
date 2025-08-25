// This is necessary to make functions like popen() and pclose() available.
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "xrandr_parser.h"

/**
 * @brief Prints the details of all parsed displays.
 * I just use it for testing
 * @param displays An array of Display structs.
 * @param count The number of displays in the array.
 */
void print_displays(const Display *displays, int count) {
    for (int i = 0; i < count; i++) {
        printf("\nDisplay #%d:\n", i + 1);
        printf("  Name: %s\n", displays[i].name);
        printf("  Connected: %s\n", displays[i].connected ? "Yes" : "No");

        if (displays[i].connected) {
            printf("  Primary: %s\n", displays[i].is_primary ? "Yes" : "No");
            if (displays[i].width > 0) {
                 printf("  Current Resolution: %dx%d at +%d+%d\n", displays[i].width, displays[i].height, displays[i].x_offset, displays[i].y_offset);
            }
            printf("  Available modes (%d):\n", displays[i].mode_count);
            for (int j = 0; j < displays[i].mode_count; j++) {
                Mode *mode = &displays[i].modes[j];
                printf("    - %dx%d (Refresh rates:", mode->width, mode->height);
                for (int k = 0; k < mode->rate_count; k++) {
                    RefreshRate *rate = &mode->refresh_rates[k];
                    printf(" %.2f", rate->rate);
                    if (rate->is_current) printf("*");
                    if (rate->is_preferred) printf("+");
                }
                printf(")\n");
            }
        }
    }
}

/**
 * @brief Frees all dynamically allocated memory for the display data.
 * @param displays An array of Display structs.
 * @param count The number of displays in the array.
 */
void free_displays(Display *displays, int count) {
    if (displays == NULL) return;

    for (int i = 0; i < count; i++) {
        for (int j = 0; j < displays[i].mode_count; j++) {
            free(displays[i].modes[j].refresh_rates);
        }
        free(displays[i].modes);
    }
    free(displays);
}

/**
 * @brief Executes xrandr, parses its output, and returns structured display info.
 * @param display_count Pointer to an integer that will be filled with the number of displays found.
 * @return A dynamically allocated array of Display structs. Don't forget to free this memory with free_displays().
 */
Display* parse_xrandr_output(int *display_count) {
    FILE *fp;
    char line[256];
    Display *displays = NULL;
    *display_count = 0;
    Display *current_display_ptr = NULL;

    // Run the xrandr command and open a pipe to read. Try and do both with popen()
    fp = popen("xrandr", "r");
    if (fp == NULL) {
        perror("Failed to run xrandr command");
        return NULL;
    }

    // Read the output line by line.
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Check for lines that describe a display connection
        if (strstr(line, " connected")) {
            (*display_count)++;
            Display *temp_displays = realloc(displays, (*display_count) * sizeof(Display));
            if (temp_displays == NULL) {
                perror("Failed to reallocate memory for displays");
                free_displays(displays, (*display_count) - 1);
                pclose(fp);
                return NULL;
            }
            displays = temp_displays;
            current_display_ptr = &displays[(*display_count) - 1];
            memset(current_display_ptr, 0, sizeof(Display));

            if (strstr(line, " connected")) {
                current_display_ptr->connected = 1;
            } else {
                current_display_ptr->connected = 0;
                sscanf(line, "%s disconnected", current_display_ptr->name);
                continue; // Nothing more to parse for a disconnected display
            }

            // Do a more robust parser later
            if (strstr(line, " primary")) {
                current_display_ptr->is_primary = 1;
                sscanf(line, "%s connected primary %dx%d+%d+%d",
                       current_display_ptr->name,
                       &current_display_ptr->width, &current_display_ptr->height,
                       &current_display_ptr->x_offset, &current_display_ptr->y_offset);
            } else {
                current_display_ptr->is_primary = 0;
                sscanf(line, "%s connected %dx%d+%d+%d",
                       current_display_ptr->name,
                       &current_display_ptr->width, &current_display_ptr->height,
                       &current_display_ptr->x_offset, &current_display_ptr->y_offset);
            }

        } else if (current_display_ptr && current_display_ptr->connected && isspace(line[0])) {
            // Pvz: "   1920x1080     60.01*+  59.97    59.96    59.93  "
            int w, h, n;
            // Use " %dx%d" to skip leading whitespace and %n to find where the resolution part ends.
            if (sscanf(line, " %dx%d %n", &w, &h, &n) == 2) {
                // new mode was found, add it to the current display
                current_display_ptr->mode_count++;
                Mode *temp_modes = realloc(current_display_ptr->modes, current_display_ptr->mode_count * sizeof(Mode));
                if (temp_modes == NULL) { /* Handle error */ exit(1); }
                current_display_ptr->modes = temp_modes;

                Mode *current_mode = &current_display_ptr->modes[current_display_ptr->mode_count - 1];
                memset(current_mode, 0, sizeof(Mode));
                current_mode->width = w;
                current_mode->height = h;

                char *ptr = &line[n]; // Start parsing for refresh rates from here
                char *endptr;

                // Loop through the rest of the line to find all refresh rates
                while (1) {
                    // strtod converts string to double, skipping leading whitespace,
                    // and updates endptr to point after the parsed number.
                    double rate_val = strtod(ptr, &endptr);

                    // If no conversion was made, we've reached the end of the numbers.
                    if (ptr == endptr) {
                        break;
                    }

                    // A new refresh rate was found, add it to the current mode
                    current_mode->rate_count++;
                    RefreshRate *temp_rates = realloc(current_mode->refresh_rates, current_mode->rate_count * sizeof(RefreshRate));
                    if (temp_rates == NULL) { /* Handle error later */ exit(1); }
                    current_mode->refresh_rates = temp_rates;

                    RefreshRate *current_rate = &current_mode->refresh_rates[current_mode->rate_count - 1];
                    memset(current_rate, 0, sizeof(RefreshRate));
                    current_rate->rate = rate_val;

                    ptr = endptr; // ptr now points to potential markers like '*' or '+'

                    // accounting for variable whitespace and handle the '+' '*' 
                    while (1) {
                        // Skip any whitespace between the number and a potential marker.
                        while (*ptr && isspace((unsigned char)*ptr)) {
                            ptr++;
                        }

                        if (*ptr == '*') {
                            current_rate->is_current = 1;
                            ptr++; // Consume the marker
                        } else if (*ptr == '+') {
                            current_rate->is_preferred = 1;
                            ptr++; // Consume the marker
                        } else {
                            // break the marker-parsing loop
                            break;
                        }
                    }
                }
            }
        } else {
            // It could be the "Screen 0: ..." line or a blank line or smth else
            current_display_ptr = NULL;
        }
    }

    pclose(fp);
    return displays;
}
