{
    function a() { a() }
}
// ----
// : movable, movableIfStorageInvariant, sideEffectFree, sideEffectFreeIfNoMSize
// a:
