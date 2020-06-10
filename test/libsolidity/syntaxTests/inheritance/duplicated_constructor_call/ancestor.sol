contract A { constructor(uint) { } }
contract B is A(2) { constructor() {  } }
contract C is B { constructor() A(3) {  } }
// ----
// DeclarationError: (125-129): Base constructor arguments given twice.
