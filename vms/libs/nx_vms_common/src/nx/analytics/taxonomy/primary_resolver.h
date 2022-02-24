// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::api::analytics { struct Descriptors; }

namespace nx::analytics::taxonomy {

class ErrorHandler;

class PrimaryResolver
{
public:
    static void resolve(
        nx::vms::api::analytics::Descriptors* inOutDescriptors, ErrorHandler* errorHandler);
};

} // namespace nx::analytics::taxonomy
