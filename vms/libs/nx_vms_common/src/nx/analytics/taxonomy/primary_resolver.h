// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::api::analytics { struct Descriptors; }

namespace nx::analytics::taxonomy {

class ErrorHandler;

class PrimaryResolver
{
public:
    // It removes descriptors with invalid id/flags and cleans absent/cycled base descriptors.
    static void resolve(
        nx::vms::api::analytics::Descriptors* inOutDescriptors, ErrorHandler* errorHandler);
};

} // namespace nx::analytics::taxonomy
