// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**
 * Generates static const structName_fieldName_str with "fieldName" value 
 * and checks in compile time that specified field actually exists.
 */
#define MAKE_FIELD_NAME_STR_CONST(structName, fieldName)                    \
    static const char* structName##_##fieldName##_##field = #fieldName;     \
    static const auto structName##_##fieldName##_##existenceCheck = &structName::fieldName;
