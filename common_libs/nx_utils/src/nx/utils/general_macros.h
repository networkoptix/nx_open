#pragma once

/**
 * Needed as a workaround for an MSVC issue: if __VA_ARGS__ is used as an argument to another
 * macro, it forms a single macro argument even if contains commas.
 */
#define NX_MSVC_EXPAND(x) x

/** Useful to convert a single macro arg which has form (A, B, ...) to arg list: A, B, ... */
#define NX_REMOVE_PARENTHESES(...) __VA_ARGS__

/**
 * Useful for macros argument overload.
 * ```
 * #define M1(x) ...
 * #define M2(x, y) ...
 * #define M(...) NX_MSVC_EXPAND(NX_GET_3RD_ARG(__VA_ARGS__, M1, M2, args_required)(__VA_ARGS__))
 * ```
 */
#define NX_GET_2ND_ARG(a1, a2, ...) a2
#define NX_GET_3RD_ARG(a1, a2, a3, ...) a3
#define NX_GET_4TH_ARG(a1, a2, a3, a4, ...) a4
#define NX_GET_5TH_ARG(a1, a2, a3, a4, a5, ...) a5
#define NX_GET_6TH_ARG(a1, a2, a3, a4, a5, a6, ...) a6
#define NX_GET_7TH_ARG(a1, a2, a3, a4, a5, a6, a7, ...) a7
#define NX_GET_8TH_ARG(a1, a2, a3, a4, a5, a6, a7, a8, ...) a8
#define NX_GET_9TH_ARG(a1, a2, a3, a4, a5, a6, a7, a8, a9, ...) a9
#define NX_GET_10TH_ARG(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10
#define NX_GET_11TH_ARG(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
#define NX_GET_12TH_ARG(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, ...) a12
#define NX_GET_13TH_ARG(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, ...) a13
#define NX_GET_14TH_ARG(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, ...) a14
#define NX_GET_15TH_ARG(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, ...) a15

/** Concatanate MSVC workadound. */
#define NX_DIRECT_CONCATENATE(s1, s2) s1 ## s2
#define NX_CONCATENATE(s1, s2) NX_DIRECT_CONCATENATE(s1, s2)
