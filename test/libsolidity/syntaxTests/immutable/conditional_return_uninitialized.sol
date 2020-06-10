contract C {
    uint immutable x;
    constructor() {
        if (false)
            return;

        x = 1;
    }
}
// ----
// TypeError: (86-93): Construction control flow ends without initializing all immutable state variables.
