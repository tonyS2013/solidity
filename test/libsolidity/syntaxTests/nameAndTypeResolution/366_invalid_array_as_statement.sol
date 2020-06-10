contract test {
    struct S { uint x; }
    constructor(uint k) { S[k]; }
}
// ----
// TypeError: (69-70): Integer constant expected.
