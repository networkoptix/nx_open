// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * This header is to be used by Qt MOC only!
 *
 * It contains dummy replacements for enum declaration macros from nx_reflect and makes MOC
 * understand enums declared with these macros with no need to properly include all project
 * headers. We don't want to include everything because it slows down MOC significantly. Also we
 * can't add only nx_reflect to MOC include paths because its headers could be included indirectly.
 */

#define NX_REFLECTION_ENUM_CLASS(ENUM, ...) \
    enum class ENUM { __VA_ARGS__ };

#define NX_REFLECTION_ENUM(ENUM, ...) \
    enum ENUM { __VA_ARGS__ };

#define NX_REFLECTION_ENUM_CLASS_IN_CLASS(ENUM, ...) \
    enum class ENUM { __VA_ARGS__ };

#define NX_REFLECTION_ENUM_IN_CLASS(ENUM, ...) \
    enum ENUM { __VA_ARGS__ };
