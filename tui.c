#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> // For SIGWINCH
#include <stdbool.h> // For bool type
#include "xrandr_parser.h"

// Minimum terminal dimensions required for the TUI
#define MIN_ROWS 20
#define MIN_COLS 80


// Determining which panel is active.
typedef enum {
    STATE_MONITOR_SELECT,
    STATE_MODE_SELECT,
    STATE_RATE_SELECT
} AppState;

// modified in a signal handler.
volatile sig_atomic_t resized_flag = 0;

/**
 * @brief Signal handler for SIGWINCH (window resize).
 *
 * This function simply sets a flag that the main loop can check.
 * @param sig The signal number (unused).
 */
void handle_winch(int sig) {
    (void)sig; // Unused parameter
    resized_flag = 1;
}

/**
 * @brief Initializes ncurses with standard settings.
 */
void init_ncurses() {
    initscr();            
    cbreak();             
    noecho();             
    keypad(stdscr, TRUE); 
    curs_set(0);          
    timeout(100);         
}

/**
 * @brief Cleans up and closes ncurses.
 */
void cleanup_ncurses() {
    endwin(); 
}

/**
 * @brief Draws the main UI border and title.
 * @param rows Total rows of the terminal.
 * @param cols Total columns of the terminal.
 */
void draw_border(int rows, int cols, AppState state) {
    (void)cols; // I kept it for potential future use.
    box(stdscr, 0, 0);
    mvprintw(0, 2, " myrandr - Display Manager ");

    const char* help_text;
    switch(state) {
        case STATE_MODE_SELECT:
            help_text = "j/k: Select Mode | h/Left: Back | l/Right/Enter: Select Rate | q: Quit";
            break;
        case STATE_RATE_SELECT:
            help_text = "j/k: Select Rate | h/Left: Back | Enter: Apply | q: Quit";
            break;
        case STATE_MONITOR_SELECT:
        default:
            help_text = "j/k: Select Display | l/Right/Enter: Select Modes | q: Quit";
            break;
    }
    mvprintw(rows - 1, 2, " %s ", help_text);
}

/**
 * @brief Draws the list of monitors on the left.
 * @param items An array of strings for the menu items.
 * @param count The number of items in the menu.
 * @param highlight The index of the currently selected item.
 * @param is_active True if this panel is being actively navigated.
 * @param scroll_offset The starting index for the visible list.
 * @param view_height The maximum number of items to display.
 */
void draw_monitor_list(const char *items[], int count, int highlight, bool is_active, int scroll_offset, int view_height) {
    int x = 2;
    int y = 2;
    mvprintw(y, x, "DISPLAYS:");
    y++;

    for (int i = 0; i < view_height && (scroll_offset + i) < count; i++) {
        int item_index = scroll_offset + i;
        if (item_index == highlight) {
            // If this list is active, reverse. If not, just make it bold.
            wattron(stdscr, is_active ? A_REVERSE : A_BOLD);
        }
        mvprintw(y + i, x + 2, "%s", items[item_index]);
        if (item_index == highlight) {
            wattroff(stdscr, is_active ? A_REVERSE : A_BOLD);
        }
    }
}

/**
 * @brief Draws the right-hand panel, which shows display info, modes, and rates.
 */
void draw_right_panel(const Display *display, AppState state, int mode_highlight, int rate_highlight, int mode_scroll, int rate_scroll, int rows, int cols) {
    int start_col = cols / 3;
    int y = 2;

    // Separator
    mvvline(1, start_col - 2, ACS_VLINE, rows - 2);

    // Basic Info
    mvprintw(y++, start_col, "Display: %s (%s)", display->name, display->is_primary ? "Primary" : "Secondary");
    if (display->width > 0) {
        // Find the current refresh rate by looking for the '*' marker
        double current_rate = 0.0;
        bool rate_found = false;
        for (int i = 0; i < display->mode_count; i++) {
            for (int j = 0; j < display->modes[i].rate_count; j++) {
                if (display->modes[i].refresh_rates[j].is_current) {
                    current_rate = display->modes[i].refresh_rates[j].rate;
                    rate_found = true;
                    break;
                }
            }
            if (rate_found) {
                break;
            }
        }

        if (rate_found) {
            mvprintw(y++, start_col, "Current: %dx%d+%d+%d @ %.2fHz", display->width, display->height, display->x_offset, display->y_offset, current_rate);
        } else {
            mvprintw(y++, start_col, "Current: %dx%d+%d+%d", display->width, display->height, display->x_offset, display->y_offset);
        }
    }
    y++;

    if (state == STATE_MONITOR_SELECT) {
        mvprintw(y, start_col, "Press 'l' or Enter to see modes.");
        return;
    }

    // --- Mode List ---
    int mode_col = start_col;
    int mode_y = y;
    mvprintw(mode_y++, mode_col, "Modes:");
    
    bool modes_active = (state == STATE_MODE_SELECT);
    if (!modes_active && state == STATE_RATE_SELECT) wattron(stdscr, A_DIM);

    int list_view_height = rows - 1 - mode_y; // Calculate available vertical space

    for (int i = 0; i < list_view_height && (mode_scroll + i) < display->mode_count; i++) {
        int item_index = mode_scroll + i;
        if (item_index == mode_highlight) wattron(stdscr, modes_active ? A_REVERSE : A_BOLD);
        mvprintw(mode_y + i, mode_col + 2, "%dx%d", display->modes[item_index].width, display->modes[item_index].height);
        if (item_index == mode_highlight) wattroff(stdscr, modes_active ? A_REVERSE : A_BOLD);
    }
    if (!modes_active && state == STATE_RATE_SELECT) wattroff(stdscr, A_DIM);

    if (state != STATE_RATE_SELECT) return;

    // --- Rate List ---
    if (display->mode_count == 0 || mode_highlight >= display->mode_count) return;
    Mode* selected_mode = &display->modes[mode_highlight];

    int rate_col = start_col + 18; // Position to the right of modes
    int rate_y = y;
    mvprintw(rate_y++, rate_col, "Refresh Rates:");

    for (int i = 0; i < list_view_height && (rate_scroll + i) < selected_mode->rate_count; i++) {
        int item_index = rate_scroll + i;
        if (item_index == rate_highlight) wattron(stdscr, A_REVERSE);
        mvprintw(rate_y + i, rate_col + 2, "%.2fHz%s%s", selected_mode->refresh_rates[item_index].rate, selected_mode->refresh_rates[item_index].is_current ? "*" : "", selected_mode->refresh_rates[item_index].is_preferred ? "+" : "");
        if (item_index == rate_highlight) wattroff(stdscr, A_REVERSE);
    }
}

/**
 * @brief Executes the xrandr command to apply the selected settings.
 * @param display The target display.
 * @param mode The target mode (resolution).
 * @param rate The target refresh rate.
 */
void apply_xrandr_settings(const Display* display, const Mode* mode, const RefreshRate* rate) {
    char command[256];
    snprintf(command, sizeof(command), "xrandr --output %s --mode %dx%d --rate %.2f",
             display->name, mode->width, mode->height, rate->rate);

    // Temporarily leave ncurses to run the command and see its output
    def_prog_mode(); // Save ncurses terminal state
    //somehow didn't work
    endwin();        

    printf("Running command: %s\n", command);
    system(command); 
    printf("Press Enter to return to the application.");
    getchar(); // Wait for user 

    reset_prog_mode(); // Restore terminal state
}

/**
 * @brief Displays a message when the terminal is too small.
 */
void draw_resize_message() {
    clear();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    const char *message1 = "Terminal too small!";
    const char *message2 = "Please resize to at least";
    char size_req[50];
    snprintf(size_req, sizeof(size_req), "%d rows x %d cols", MIN_ROWS, MIN_COLS);

    mvprintw(rows / 2 - 2, (cols - strlen(message1)) / 2, "%s", message1);
    mvprintw(rows / 2 - 1, (cols - strlen(message2)) / 2, "%s", message2);
    mvprintw(rows / 2, (cols - strlen(size_req)) / 2, "%s", size_req);
    refresh();
}

/**
 * @brief Frees memory associated with the dynamic menu and display lists.
 */
void cleanup_display_data(Display *displays, int display_count, char **menu_items, Display **connected_displays) {
    free(menu_items);
    free(connected_displays);
    free_displays(displays, display_count);
}

/**
 * @brief Parses xrandr output and sets up all data structures for the TUI.
 * @return True on success, false on failure.
 */
bool setup_display_data(Display **displays, int *display_count, 
                        char ***menu_items, int *num_items,
                        Display ***connected_displays, int *connected_count) {
    *displays = parse_xrandr_output(display_count);
    if (*displays == NULL) {
        fprintf(stderr, "Failed to parse xrandr output. Is xrandr installed and in your PATH?\n");
        return false;
    }

    *connected_count = 0;
    for (int i = 0; i < *display_count; i++) {
        if ((*displays)[i].connected) {
            (*connected_count)++;
        }
    }

    *menu_items = malloc((*connected_count + 1) * sizeof(char *));
    *connected_displays = malloc(*connected_count * sizeof(Display*));

    if (*menu_items == NULL || *connected_displays == NULL) {
        fprintf(stderr, "Failed to allocate memory for menu.\n");
        free_displays(*displays, *display_count);
        free(*menu_items);
        free(*connected_displays);
        return false;
    }

    int current_item = 0;
    for (int i = 0; i < *display_count; i++) {
        if ((*displays)[i].connected) {
            (*menu_items)[current_item] = (*displays)[i].name;
            (*connected_displays)[current_item] = &(*displays)[i];
            current_item++;
        }
    }
    (*menu_items)[*connected_count] = "Exit";
    *num_items = *connected_count + 1;

    return true;
}

int main() {
    Display *displays = NULL;
    int display_count = 0;
    char **menu_items = NULL;
    int num_items = 0;
    Display **connected_displays = NULL;
    int connected_count = 0;

    if (!setup_display_data(&displays, &display_count, &menu_items, &num_items, &connected_displays, &connected_count)) {
        return 1;
    }

    AppState state = STATE_MONITOR_SELECT;
    int monitor_highlight = 0;
    int mode_highlight = 0;
    int rate_highlight = 0;
    int monitor_scroll = 0;
    int mode_scroll = 0;
    int rate_scroll = 0;
    // Register the signal handler for window resize, remember to make it work
    signal(SIGWINCH, handle_winch);

    init_ncurses();

    int rows, cols;

    // --- Main Application Loop ---
    while (1) {
        // Get terminal size
        getmaxyx(stdscr, rows, cols);

        // --- Handle Resize ---
        if (resized_flag) {
            // ncurses' own resize handling function, doesn't seem to work
            resizeterm(rows, cols);
            resized_flag = 0;
            clear(); // Clear the screen to avoid artifacts
        }

        // --- Check Minimum Size ---
        if (rows < MIN_ROWS || cols < MIN_COLS) {
            draw_resize_message();
            // Wait for the next event (a key press or another resize)
            getch();
            continue; // Skip the loop and re-evaluate
        }

        // --- Drawing ---
        clear();
        int monitor_view_height = rows - 4; // border + title
        draw_border(rows, cols, state);
        draw_monitor_list((const char **)menu_items, num_items, monitor_highlight, state == STATE_MONITOR_SELECT, monitor_scroll, monitor_view_height);
        
        // If a display is highlighted (and it's not 'Exit'), show its info
        if (monitor_highlight < connected_count) {
            draw_right_panel(connected_displays[monitor_highlight], state, 
                             mode_highlight, rate_highlight, mode_scroll, rate_scroll,
                             rows, cols);
        } else {
            // Give info for the 'Exit' option when it's highlighted
            mvprintw(4, cols / 2, "Select to quit the application.");
        }

        refresh();

        // --- Input Handling ---
        int ch = getch(); // Blocks until key press or timeout

        switch (ch) {
            case 'q':
            case 'Q':
                goto end_loop;

            case KEY_UP:
            case 'k':
                { // Use a block to create block-scoped variables
                    int right_panel_view_height = rows - 8; // Approximate height for right-side lists
                    if (right_panel_view_height < 1) right_panel_view_height = 1;

                    if (state == STATE_MONITOR_SELECT) {
                        monitor_highlight = (monitor_highlight == 0) ? num_items - 1 : monitor_highlight - 1;
                        if (monitor_highlight < monitor_scroll) {
                            monitor_scroll = monitor_highlight;
                        } else if (monitor_highlight >= num_items - 1) { // Wrapped to bottom
                            monitor_scroll = (num_items > monitor_view_height) ? num_items - monitor_view_height : 0;
                        }
                    } else if (state == STATE_MODE_SELECT) {
                        Display* d = connected_displays[monitor_highlight];
                        if (d->mode_count > 0) {
                            mode_highlight = (mode_highlight == 0) ? d->mode_count - 1 : mode_highlight - 1;
                            if (mode_highlight < mode_scroll) {
                                mode_scroll = mode_highlight;
                            } else if (mode_highlight >= d->mode_count - 1) { // Wrapped to bottom
                                mode_scroll = (d->mode_count > right_panel_view_height) ? d->mode_count - right_panel_view_height : 0;
                            }
                        }
                    } else { // STATE_RATE_SELECT
                        Display* d = connected_displays[monitor_highlight];
                        if (d->mode_count > 0) {
                            Mode* m = &d->modes[mode_highlight];
                            if (m->rate_count > 0) {
                                rate_highlight = (rate_highlight == 0) ? m->rate_count - 1 : rate_highlight - 1;
                                if (rate_highlight < rate_scroll) {
                                    rate_scroll = rate_highlight;
                                } else if (rate_highlight >= m->rate_count - 1) { // Wrapped to bottom
                                    rate_scroll = (m->rate_count > right_panel_view_height) ? m->rate_count - right_panel_view_height : 0;
                                }
                            }
                        }
                    }
                }
                break;

            case KEY_DOWN:
            case 'j':
                { // Use a block to create block-scoped variables
                    int right_panel_view_height = rows - 8; // Approximate height for right-side lists
                    if (right_panel_view_height < 1) right_panel_view_height = 1;

                    if (state == STATE_MONITOR_SELECT) {
                        monitor_highlight = (monitor_highlight + 1) % num_items;
                        if (monitor_highlight >= monitor_scroll + monitor_view_height) {
                            monitor_scroll = monitor_highlight - monitor_view_height + 1;
                        } else if (monitor_highlight == 0) { // Wrapped around
                            monitor_scroll = 0;
                        }
                    } else if (state == STATE_MODE_SELECT) {
                        Display* d = connected_displays[monitor_highlight];
                        if (d->mode_count > 0) {
                            mode_highlight = (mode_highlight + 1) % d->mode_count;
                            if (mode_highlight >= mode_scroll + right_panel_view_height) {
                                mode_scroll = mode_highlight - right_panel_view_height + 1;
                            } else if (mode_highlight == 0) { // Wrapped around
                                mode_scroll = 0;
                            }
                        }
                    } else { // STATE_RATE_SELECT
                        Display* d = connected_displays[monitor_highlight];
                        if (d->mode_count > 0) {
                            Mode* m = &d->modes[mode_highlight];
                            if (m->rate_count > 0) {
                                rate_highlight = (rate_highlight + 1) % m->rate_count;
                                if (rate_highlight >= rate_scroll + right_panel_view_height) {
                                    rate_scroll = rate_highlight - right_panel_view_height + 1;
                                } else if (rate_highlight == 0) { // Wrapped around
                                    rate_scroll = 0;
                                }
                            }
                        }
                    }
                }
                break;

            case KEY_RIGHT:
            case 'l':
                if (state == STATE_MONITOR_SELECT && monitor_highlight < connected_count) {
                    state = STATE_MODE_SELECT;
                    mode_highlight = 0; mode_scroll = 0;
                    rate_highlight = 0; rate_scroll = 0;
                } else if (state == STATE_MODE_SELECT) {
                    Display* d = connected_displays[monitor_highlight];
                    if (d->mode_count > 0) {
                        state = STATE_RATE_SELECT;
                        rate_highlight = 0; rate_scroll = 0;
                    }
                }
                break;

            case KEY_LEFT:
            case 'h':
                if (state == STATE_RATE_SELECT) {
                    state = STATE_MODE_SELECT;
                    rate_highlight = 0; rate_scroll = 0;
                } else if (state == STATE_MODE_SELECT) {
                    state = STATE_MONITOR_SELECT;
                    mode_highlight = 0; mode_scroll = 0;
                }
                break;

            case 10: // Enter key
                if (state == STATE_MONITOR_SELECT) {
                    if (monitor_highlight == connected_count) goto end_loop; // "Exit" selected
                    state = STATE_MODE_SELECT;
                    mode_highlight = 0; mode_scroll = 0;
                    rate_highlight = 0; rate_scroll = 0;
                } else if (state == STATE_MODE_SELECT && connected_displays[monitor_highlight]->mode_count > 0) {
                    state = STATE_RATE_SELECT;
                    rate_highlight = 0; rate_scroll = 0;
                } else if (state == STATE_RATE_SELECT) {
                    // Get selected items
                    Display* selected_display = connected_displays[monitor_highlight];
                    Mode* selected_mode = &selected_display->modes[mode_highlight];
                    RefreshRate* selected_rate = &selected_mode->refresh_rates[rate_highlight];

                    // Apply settings
                    apply_xrandr_settings(selected_display, selected_mode, selected_rate);

                    // Clean up old data structures
                    cleanup_display_data(displays, display_count, menu_items, connected_displays);

                    // Reparse and rebuild menus with the new/updated data
                    if (!setup_display_data(&displays, &display_count, &menu_items, &num_items, &connected_displays, &connected_count)) {
                        cleanup_ncurses();
                        fprintf(stderr, "Failed to re-parse xrandr data after mode change.\n");
                        return 1;
                    }

                    // Reset UI state to the top, as data has changed
                    state = STATE_MONITOR_SELECT;
                    monitor_highlight = 0; monitor_scroll = 0;
                    mode_highlight = 0; mode_scroll = 0;
                    rate_highlight = 0; rate_scroll = 0;
                    clear(); // Force a full redraw on the next iteration
                }
                break;
            case ERR:
                // No input, timeout occurred. Loop will continue,
                // can use for checking the resize flag.
                break;
        }
    }

end_loop:

    cleanup_ncurses();

    cleanup_display_data(displays, display_count, menu_items, connected_displays);
    printf("myrandr exited cleanly.\n");

    return 0;
}
