contract A { constructor() { } }
contract B is A { constructor() A {  } }
// ----
// DeclarationError: (65-66): Modifier-style base constructor call without arguments.
