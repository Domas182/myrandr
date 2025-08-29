# myrandr

A simple, terminal-based TUI wrapper for `xrandr` to manage display settings like resolution, refresh rate, and position.



## Dependencies

Before compiling, ensure you have the following dependencies installed:

- **`xrandr`**: The command-line tool this program wraps.
- **`ncurses` library**: Required for the terminal user interface. You may need to install a development package like `libncurses-dev` (on Debian/Ubuntu) or `ncurses-devel` (on Fedora/CentOS).

## Compilation

To compile the application, navigate to the source directory and run the following command:

```bash
make
```

This will create an executable file named `myrandr`.

## Usage

Run the compiled binary from your terminal:

```bash
./myrandr
```

The application presents a list of connected displays on the left and details/options for the selected display on the right.

### Keybindings

The interface is navigated primarily using vim-like keys.

*   **General Navigation:**
    *   `j` / `k` (or `Down` / `Up`): Navigate up and down lists.
    *   `h` / `l` (or `Left` / `Right`): Move between panels or go back.
    *   `Enter`: Select an item or confirm an action.
    *   `q`: Quit the application at any time.

*   **Main Display List:**
    *   `o`: Toggle the selected display on (`--auto`) or off (`--off`).
    *   `p`: Open the positioning panel for the selected display (only available if more than one monitor is connected).

*   **Positioning Panel:**
    *   `Tab`: Switch focus between the "Target Monitor" list and the "Position" list.
    *   `Enter`: Apply the selected position (`--right-of`, `--left-of`, etc.).
