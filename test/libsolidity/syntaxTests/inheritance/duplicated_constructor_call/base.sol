contract A { constructor(uint) { } }
contract B is A(2) { constructor() A(3) {  } }
// ----
// DeclarationError: (79-83): Base constructor arguments given twice.
