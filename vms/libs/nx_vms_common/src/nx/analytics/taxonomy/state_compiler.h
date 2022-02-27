// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/common.h>
#include <nx/analytics/taxonomy/abstract_state.h>

namespace nx::analytics::taxonomy {

struct InternalState;
class ErrorHandler;

class NX_VMS_COMMON_API StateCompiler
{
public:
    struct Result
    {
        std::shared_ptr<AbstractState> state;
        std::vector<ProcessingError> errors;
    };

public:
    static Result compile(nx::vms::api::analytics::Descriptors descriptors);
};

} // namespace nx::analytics::taxonomy
