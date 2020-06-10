contract C { constructor(uint) {} }
contract A is C { constructor() C(2) {} }
contract B is C { constructor() C(2) {} }
contract D is A, B { }
// ----
// DeclarationError: (141-163): Base constructor arguments given twice.
