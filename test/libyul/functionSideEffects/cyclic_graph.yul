{
    function a() { b() }
    function b() { c() }
    function c() { b() }
}
// ----
// : movable, movableIfStorageInvariant, sideEffectFree, sideEffectFreeIfNoMSize
// a:
// b:
// c:
