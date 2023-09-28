// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// TODO: #akolesnikov. Replace this with nx::reflect::urlencoded

/**
 * Generates static const structName_fieldName_str with "fieldName" value
 * and checks in compile time that specified field actually exists.
 */
#define MAKE_FIELD_NAME_STR_CONST(structName, fieldName)                    \
    static const char* structName##_##fieldName##_##field = #fieldName;     \
    [[maybe_unused]] static const auto structName##_##fieldName##_##existenceCheck = &structName::fieldName;
