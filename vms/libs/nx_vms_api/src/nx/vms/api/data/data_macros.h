// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>

#define NX_VMS_API_DECLARE_LIST(Type) \
    using Type##List = std::vector<Type>;

#define NX_VMS_API_DECLARE_STRUCT_EX(Type, Functions) \
    struct Type; \
    QN_FUSION_DECLARE_FUNCTIONS(Type, Functions, NX_VMS_API)

#define NX_VMS_API_DECLARE_STRUCT(Type) \
    NX_VMS_API_DECLARE_STRUCT_EX(Type, (ubjson)(json)(xml)(sql_record)(csv_record))

#define NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(Type, Functions) \
    NX_VMS_API_DECLARE_STRUCT_EX(Type, Functions) \
    NX_VMS_API_DECLARE_LIST(Type)

#define NX_VMS_API_DECLARE_STRUCT_AND_LIST(Type) \
    NX_VMS_API_DECLARE_STRUCT(Type) \
    NX_VMS_API_DECLARE_LIST(Type)
