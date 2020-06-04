from rule import Rule
from opcodes import *

"""
byte(A, shl(B, X))
given A < 32 && B == 256 - 8 * (A + 1)
->
and(X, 0xff)
"""

rule = Rule()

n_bits = 256

# Input vars
X = BitVec('X', n_bits)
A = BitVec('A', n_bits)
B = BitVec('B', n_bits)

# Non optimized result
nonopt = BYTE(A, SHL(B, X))
# Optimized result
opt = AND(X, 0xff)

rule.require(ULT(A, 32))
rule.require(B == 256 - 8 * (A + 1))

rule.check(nonopt, opt)
