{
  let b := 1
  for { let a := 1 } iszero(eq(a, 10)) {sstore(0x00, 0x00)} {
    let inv := add(b, 42)
    let x := extcodesize(mul(inv, 3))
    a := add(x, 1)
    mstore(a, inv)
  }

  for { let a := 1 } iszero(eq(a, 10)) { a := add(a, 1) } {
    let inv := add(b, 42)
    pop(call(0x01, 0x02, 0x08, 0x08, 0x02, 0x01, 0x01)) // invalidates state
    let x := extcodehash(mul(inv, 3))
    a := add(x, 1)
    mstore(a, inv)
  }

  for { let a := 1 } iszero(eq(a, 10)) { a := add(a, 1) } {
    let inv := add(b, 42)
    pop(create(0x08, 0x00, 0x02)) // invalidates state
    let x := sload(mul(inv, 3))
    a := add(x, 1)
    mstore(a, inv)
  }
}
// ----
// step: loopInvariantCodeMotion
//
// {
//     let b := 1
//     let a := 1
//     let inv := add(b, 42)
//     for { } iszero(eq(a, 10)) { sstore(0x00, 0x00) }
//     {
//         let x := extcodesize(mul(inv, 3))
//         a := add(x, 1)
//         mstore(a, inv)
//     }
//     let a_1 := 1
//     let inv_2 := add(b, 42)
//     for { } iszero(eq(a_1, 10)) { a_1 := add(a_1, 1) }
//     {
//         pop(call(0x01, 0x02, 0x08, 0x08, 0x02, 0x01, 0x01))
//         let x_3 := extcodehash(mul(inv_2, 3))
//         a_1 := add(x_3, 1)
//         mstore(a_1, inv_2)
//     }
//     let a_4 := 1
//     let inv_5 := add(b, 42)
//     for { } iszero(eq(a_4, 10)) { a_4 := add(a_4, 1) }
//     {
//         pop(create(0x08, 0x00, 0x02))
//         let x_6 := sload(mul(inv_5, 3))
//         a_4 := add(x_6, 1)
//         mstore(a_4, inv_5)
//     }
// }
