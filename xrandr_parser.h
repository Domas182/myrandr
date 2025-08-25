#ifndef XRANDR_PARSER_H
#define XRANDR_PARSER_H

/**
 * @brief Holds information about a specific refresh rate for a mode.
 */
typedef struct {
    double rate;
    int is_current;   // Marked with '*'
    int is_preferred; // Marked with '+'
} RefreshRate;

/**
 * @brief Holds information about a display mode (resolution).
 */
typedef struct {
    int width;
    int height;
    RefreshRate *refresh_rates;
    int rate_count;
} Mode;

/**
 * @brief Holds all information about a single display output.
 */
typedef struct {
    char name[32];
    int connected;
    int is_primary;
    // Current resolution details (if connected)
    int width;
    int height;
    int x_offset;
    int y_offset;
    // List of available modes
    Mode *modes;
    int mode_count;
} Display;

Display* parse_xrandr_output(int *display_count);
void free_displays(Display *displays, int count);

#endif // XRANDR_PARSER_H