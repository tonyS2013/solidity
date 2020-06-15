{
    function a() { b() }
    function b() { a() }
}
// ----
// : movable, movableIfStorageInvariant, sideEffectFree, sideEffectFreeIfNoMSize
// a:
// b:
