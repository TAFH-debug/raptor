#pragma once

#define FE_EXPAND(x) x

#define FE_1(m, x)       m(x)
#define FE_2(m, x, ...)  m(x) FE_EXPAND(FE_1(m, __VA_ARGS__))
#define FE_3(m, x, ...)  m(x) FE_EXPAND(FE_2(m, __VA_ARGS__))
#define FE_4(m, x, ...)  m(x) FE_EXPAND(FE_3(m, __VA_ARGS__))
#define FE_5(m, x, ...)  m(x) FE_EXPAND(FE_4(m, __VA_ARGS__))
#define FE_6(m, x, ...)  m(x) FE_EXPAND(FE_5(m, __VA_ARGS__))
#define FE_7(m, x, ...)  m(x) FE_EXPAND(FE_6(m, __VA_ARGS__))
#define FE_8(m, x, ...)  m(x) FE_EXPAND(FE_7(m, __VA_ARGS__))
#define FE_9(m, x, ...)  m(x) FE_EXPAND(FE_8(m, __VA_ARGS__))
#define FE_10(m, x, ...) m(x) FE_EXPAND(FE_9(m, __VA_ARGS__))
#define FE_11(m, x, ...) m(x) FE_EXPAND(FE_10(m, __VA_ARGS__))
#define FE_12(m, x, ...) m(x) FE_EXPAND(FE_11(m, __VA_ARGS__))
#define FE_13(m, x, ...) m(x) FE_EXPAND(FE_12(m, __VA_ARGS__))
#define FE_14(m, x, ...) m(x) FE_EXPAND(FE_13(m, __VA_ARGS__))
#define FE_15(m, x, ...) m(x) FE_EXPAND(FE_14(m, __VA_ARGS__))
#define FE_16(m, x, ...) m(x) FE_EXPAND(FE_15(m, __VA_ARGS__))

#define FE_PICK(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16, NAME, ...) NAME

#define FOR_EACH(m, ...) FE_EXPAND(FE_PICK(__VA_ARGS__, \
    FE_16,FE_15,FE_14,FE_13,FE_12,FE_11,FE_10,FE_9,     \
    FE_8,FE_7,FE_6,FE_5,FE_4,FE_3,FE_2,FE_1)(m, __VA_ARGS__))
    