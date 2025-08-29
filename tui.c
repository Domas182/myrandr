#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> // For bool type
#include "xrandr_parser.h"

// Minimum terminal dimensions required for the TUI
#define MIN_ROWS 20
#define MIN_COLS 80


// Determining which panel is active.
typedef enum {
    STATE_MONITOR_SELECT,
    STATE_MODE_SELECT,
    STATE_RATE_SELECT,
    STATE_POSITION_SELECT
} AppState;

/**
 * @brief For the position selection panel, which sub-panel is active.
 */
typedef enum {
    POS_PANEL_TARGET,
    POS_PANEL_DIRECTION
} PositionPanelFocus;

/**
 * @brief Initializes ncurses with standard settings.
 */
void init_ncurses() {
    initscr();            
    cbreak();             
    noecho();             
    keypad(stdscr, TRUE); 
    curs_set(0);          
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
        case STATE_POSITION_SELECT:
            help_text = "j/k: Select | Tab: Switch | h/Left: Back | Enter: Apply | q: Quit";
            break;
        case STATE_RATE_SELECT:
            help_text = "j/k: Select Rate | h/Left: Back | Enter: Apply | q: Quit";
            break;
        case STATE_MONITOR_SELECT:
        default:
            help_text = "j/k: Select Display | o: On/Off | p: Position | l/Right/Enter: Modes | q: Quit";
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
void draw_monitor_list(Display *displays[], int display_count, int total_items, int highlight, bool is_active, int scroll_offset, int view_height) {
    int x = 2;
    int y = 2;
    mvprintw(y, x, "DISPLAYS:");
    y++;

    for (int i = 0; i < view_height && (scroll_offset + i) < total_items; i++) {
        int item_index = scroll_offset + i;
        if (item_index == highlight) {
            // If this list is active, reverse. If not, just make it bold.
            wattron(stdscr, is_active ? A_REVERSE : A_BOLD);
        }
        if (item_index < display_count) {
            Display* d = displays[item_index];
            mvprintw(y + i, x + 2, "%s [%s]", d->name, d->is_active ? "On" : "Off");
        } else {
            mvprintw(y + i, x + 2, "Exit");
        }
        if (item_index == highlight) {
            wattroff(stdscr, is_active ? A_REVERSE : A_BOLD);
        }
    }
}

void draw_position_panel(
    const Display* source_display,
    Display** target_displays, int target_count,
    const char** directions, int direction_count,
    int target_highlight, int direction_highlight,
    PositionPanelFocus active_panel,
    int y, int start_col)
{
    mvprintw(y++, start_col, "Positioning '%s' relative to:", source_display->name);
    y++;

    int target_col = start_col;
    int dir_col = start_col + 20;

    // Draw target monitors list
    mvprintw(y, target_col, "Target Monitor:");
    bool target_active = (active_panel == POS_PANEL_TARGET);
    if (!target_active) wattron(stdscr, A_DIM);
    for (int i = 0; i < target_count; i++) {
        if (i == target_highlight) wattron(stdscr, target_active ? A_REVERSE : A_BOLD);
        mvprintw(y + 1 + i, target_col + 2, "%s", target_displays[i]->name);
        if (i == target_highlight) wattroff(stdscr, target_active ? A_REVERSE : A_BOLD);
    }
    if (!target_active) wattroff(stdscr, A_DIM);


    // Draw direction list
    mvprintw(y, dir_col, "Position:");
    bool dir_active = (active_panel == POS_PANEL_DIRECTION);
    if (!dir_active) wattron(stdscr, A_DIM);
    for (int i = 0; i < direction_count; i++) {
        if (i == direction_highlight) wattron(stdscr, dir_active ? A_REVERSE : A_BOLD);
        mvprintw(y + 1 + i, dir_col + 2, "%s", directions[i]);
        if (i == direction_highlight) wattroff(stdscr, dir_active ? A_REVERSE : A_BOLD);
    }
    if (!dir_active) wattroff(stdscr, A_DIM);
}

/**
 * @brief Draws the right-hand panel, which shows display info, modes, and rates.
 */
void draw_right_panel(const Display *display, AppState state, int mode_highlight, int rate_highlight, int mode_scroll, int rate_scroll,
                      Display** pos_targets, int pos_target_count, int pos_target_highlight,
                      const char** pos_directions, int pos_direction_count, int pos_direction_highlight, PositionPanelFocus pos_focus,
                      int rows, int cols) {
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
        mvprintw(y++, start_col, "Press 'l' or Enter to see modes.");
        mvprintw(y, start_col, "Press 'p' to change position.");
        return;
    }

    if (state == STATE_POSITION_SELECT) {
        draw_position_panel(display, pos_targets, pos_target_count, pos_directions, pos_direction_count, pos_target_highlight, pos_direction_highlight, pos_focus, y, start_col);
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
 * @brief Toggles a display on or off using xrandr.
 * @param display The target display.
 */
void toggle_display_power(const Display* display) {
    char command[256];
    if (display->is_active) {
        snprintf(command, sizeof(command), "xrandr --output %s --off", display->name);
    } else {
        // --auto will pick the preferred mode and turn it on.
        snprintf(command, sizeof(command), "xrandr --output %s --auto", display->name);
    }

    // Temporarily leave ncurses to run the command and see its output
    def_prog_mode(); // Save ncurses terminal state
    endwin();

    printf("Running command: %s\n", command);
    system(command);
    printf("Press Enter to return to the application.");
    getchar(); // Wait for user

    reset_prog_mode(); // Restore terminal state
}

/**
 * @brief Executes the xrandr command to apply the selected position.
 * @param source_display The display to move.
 * @param target_display The reference display.
 * @param direction The relative position (e.g., "left-of").
 */
void apply_position_settings(const Display* source_display, const Display* target_display, const char* direction) {
    char command[256];
    snprintf(command, sizeof(command), "xrandr --output %s --%s %s --auto",
             source_display->name, direction, target_display->name);

    // Temporarily leave ncurses to run the command
    def_prog_mode();
    endwin();

    printf("Running command: %s\n", command);
    system(command);
    printf("Press Enter to return to the application.");
    getchar();

    reset_prog_mode();
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

    // State for positioning panel
    PositionPanelFocus pos_panel_focus = POS_PANEL_TARGET;
    int pos_target_highlight = 0;
    int pos_direction_highlight = 0;
    Display **position_target_displays = NULL;
    int position_target_count = 0;
    const char *position_directions[] = {"right-of", "left-of", "above", "below", "same-as"};
    const int position_direction_count = sizeof(position_directions) / sizeof(char*);

    init_ncurses();

    int rows, cols;
    bool needs_redraw = true;

    // --- Main Application Loop ---
    while (1) {
        if (needs_redraw) {
            getmaxyx(stdscr, rows, cols);
            clear();

            if (rows < MIN_ROWS || cols < MIN_COLS) {
                draw_resize_message();
            } else {
                int monitor_view_height = rows - 4; // border + title
                draw_border(rows, cols, state);
                draw_monitor_list(connected_displays, connected_count, num_items, monitor_highlight, state == STATE_MONITOR_SELECT, monitor_scroll, monitor_view_height);

                if (monitor_highlight < connected_count) {
                    draw_right_panel(connected_displays[monitor_highlight], state,
                                     mode_highlight, rate_highlight, mode_scroll, rate_scroll,
                                     position_target_displays, position_target_count, pos_target_highlight,
                                     (const char**)position_directions, position_direction_count, pos_direction_highlight, pos_panel_focus,
                                     rows, cols);
                } else {
                    mvprintw(4, cols / 2, "Select to quit the application.");
                }
            }
            refresh();
            needs_redraw = false;
        }

        // --- Input Handling ---
        int ch = getch(); // This now blocks until a key is pressed or window is resized.

        switch (ch) {
            case 'q':
            case 'Q':
                goto end_loop;

            case 'o':
            case 'O':
                if (state == STATE_MONITOR_SELECT && monitor_highlight < connected_count) {
                    Display* selected_display = connected_displays[monitor_highlight];
                    toggle_display_power(selected_display);

                    // Reparse and rebuild menus with the new/updated data
                    cleanup_display_data(displays, display_count, menu_items, connected_displays);
                    free(position_target_displays);
                    if (!setup_display_data(&displays, &display_count, &menu_items, &num_items, &connected_displays, &connected_count)) {
                        cleanup_ncurses();
                        fprintf(stderr, "Failed to re-parse xrandr data after toggling display.\n");
                        return 1;
                    }

                    // Reset UI state to the top, as data has changed
                    state = STATE_MONITOR_SELECT;
                    monitor_highlight = 0; monitor_scroll = 0;
                    mode_highlight = 0; mode_scroll = 0;
                    rate_highlight = 0; rate_scroll = 0;
                    needs_redraw = true;
                }
                break;

            case 'p':
            case 'P':
                if (state == STATE_MONITOR_SELECT && connected_count > 1) {
                    state = STATE_POSITION_SELECT;

                    // Build the list of target monitors (all except the selected one)
                    free(position_target_displays); // free previous list if any
                    position_target_count = connected_count - 1;
                    position_target_displays = malloc(position_target_count * sizeof(Display*));
                    if (!position_target_displays) { /* TODO: error handling */ exit(1); }

                    int current_target = 0;
                    for (int i = 0; i < connected_count; i++) {
                        if (i == monitor_highlight) continue;
                        position_target_displays[current_target++] = connected_displays[i];
                    }

                    pos_panel_focus = POS_PANEL_TARGET;
                    pos_target_highlight = 0;
                    pos_direction_highlight = 0;
                    needs_redraw = true;
                }
                break;

            case KEY_RESIZE:
                needs_redraw = true;
                break;

            case KEY_UP:
            case 'k':
                { // Use a block to create block-scoped variables
                    int monitor_view_height = rows - 4;
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
                    } else if (state == STATE_POSITION_SELECT) {
                        if (pos_panel_focus == POS_PANEL_TARGET) {
                            if (position_target_count > 0) {
                                pos_target_highlight = (pos_target_highlight == 0) ? position_target_count - 1 : pos_target_highlight - 1;
                            }
                        } else { // POS_PANEL_DIRECTION
                            pos_direction_highlight = (pos_direction_highlight == 0) ? position_direction_count - 1 : pos_direction_highlight - 1;
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
                needs_redraw = true;
                break;

            case KEY_DOWN:
            case 'j':
                { // Use a block to create block-scoped variables
                    int monitor_view_height = rows - 4;
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
                    } else if (state == STATE_POSITION_SELECT) {
                        if (pos_panel_focus == POS_PANEL_TARGET) {
                            if (position_target_count > 0) {
                                pos_target_highlight = (pos_target_highlight + 1) % position_target_count;
                            }
                        } else { // POS_PANEL_DIRECTION
                            pos_direction_highlight = (pos_direction_highlight + 1) % position_direction_count;
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
                needs_redraw = true;
                break;
            
            case 9: // Tab key
                if (state == STATE_POSITION_SELECT) {
                    pos_panel_focus = (pos_panel_focus == POS_PANEL_TARGET) ? POS_PANEL_DIRECTION : POS_PANEL_TARGET;
                    needs_redraw = true;
                }
                break;

            case KEY_RIGHT:
            case 'l':
                if (state == STATE_MONITOR_SELECT && monitor_highlight < connected_count) {
                    state = STATE_MODE_SELECT;
                    mode_highlight = 0; mode_scroll = 0;
                    rate_highlight = 0; rate_scroll = 0;
                    needs_redraw = true;
                } else if (state == STATE_MODE_SELECT) {
                    Display* d = connected_displays[monitor_highlight];
                    if (d->mode_count > 0) {
                        state = STATE_RATE_SELECT;
                        rate_highlight = 0; rate_scroll = 0;
                        needs_redraw = true;
                    }
                }
                break;

            case KEY_LEFT:
            case 'h':
                if (state == STATE_POSITION_SELECT) {
                    state = STATE_MONITOR_SELECT;
                    free(position_target_displays);
                    position_target_displays = NULL;
                    position_target_count = 0;
                    needs_redraw = true;
                } else if (state == STATE_RATE_SELECT) {
                    state = STATE_MODE_SELECT;
                    rate_highlight = 0; rate_scroll = 0;
                    needs_redraw = true;
                } else if (state == STATE_MODE_SELECT) {
                    state = STATE_MONITOR_SELECT;
                    mode_highlight = 0; mode_scroll = 0;
                    needs_redraw = true;
                }
                break;

            case 10: // Enter key
                if (state == STATE_MONITOR_SELECT) {
                    if (monitor_highlight == connected_count) goto end_loop; // "Exit" selected
                    state = STATE_MODE_SELECT;
                    mode_highlight = 0; mode_scroll = 0;
                    rate_highlight = 0; rate_scroll = 0;
                    needs_redraw = true;
                } else if (state == STATE_MODE_SELECT && connected_displays[monitor_highlight]->mode_count > 0) {
                    state = STATE_RATE_SELECT;
                    rate_highlight = 0; rate_scroll = 0;
                    needs_redraw = true;
                } else if (state == STATE_POSITION_SELECT) {
                    // Get selected items
                    Display* source_display = connected_displays[monitor_highlight];
                    Display* target_display = position_target_displays[pos_target_highlight];
                    const char* direction = position_directions[pos_direction_highlight];

                    apply_position_settings(source_display, target_display, direction);

                    cleanup_display_data(displays, display_count, menu_items, connected_displays);
                    free(position_target_displays);
                    position_target_displays = NULL;

                    if (!setup_display_data(&displays, &display_count, &menu_items, &num_items, &connected_displays, &connected_count)) {
                        cleanup_ncurses();
                        fprintf(stderr, "Failed to re-parse xrandr data after position change.\n");
                        return 1;
                    }

                    state = STATE_MONITOR_SELECT;
                    monitor_highlight = 0; monitor_scroll = 0;
                    mode_highlight = 0; mode_scroll = 0;
                    rate_highlight = 0; rate_scroll = 0;
                    needs_redraw = true;
                } else if (state == STATE_RATE_SELECT) {
                    // Get selected items
                    Display* selected_display = connected_displays[monitor_highlight];
                    Mode* selected_mode = &selected_display->modes[mode_highlight];
                    RefreshRate* selected_rate = &selected_mode->refresh_rates[rate_highlight];

                    // Apply settings
                    apply_xrandr_settings(selected_display, selected_mode, selected_rate);

                    // Clean up old data structures
                    cleanup_display_data(displays, display_count, menu_items, connected_displays);
                    free(position_target_displays);
                    position_target_displays = NULL;

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
                    needs_redraw = true;
                }
                break;
        }
    }

end_loop:

    cleanup_ncurses();

    cleanup_display_data(displays, display_count, menu_items, connected_displays);
    free(position_target_displays);
    printf("myrandr exited cleanly.\n");

    return 0;
}
