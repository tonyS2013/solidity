contract test {
    enum ActionChoices { GoLeft, GoRight, GoStraight, Sit }
    constructor() {
        b = ActionChoices.Sit;
    }
    uint64 b;
}
// ----
// TypeError: (108-125): Type enum test.ActionChoices is not implicitly convertible to expected type uint64.
