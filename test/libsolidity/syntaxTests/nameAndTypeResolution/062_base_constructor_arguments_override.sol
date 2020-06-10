contract A { constructor(uint a) { } }
contract B is A { }
// ----
// TypeError: (39-58): Contract "B" should be marked as abstract.
