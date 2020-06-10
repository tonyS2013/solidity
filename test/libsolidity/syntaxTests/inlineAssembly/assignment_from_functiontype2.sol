contract C {
    function f() public pure {}
    constructor() {
        assembly {
            let x := f
        }
    }
}
// ----
// DeclarationError: (105-106): Access to functions is not allowed in inline assembly.
