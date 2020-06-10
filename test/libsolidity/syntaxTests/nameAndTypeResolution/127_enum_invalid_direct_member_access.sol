contract test {
    enum ActionChoices { GoLeft, GoRight, GoStraight, Sit }
    constructor() {
        choices = Sit;
    }
    ActionChoices choices;
}
// ----
// DeclarationError: (114-117): Undeclared identifier.
