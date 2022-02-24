// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/analytics/taxonomy/common.h>

namespace nx::analytics::taxonomy {

class ErrorHandler
{
public:
    void handleError(ProcessingError error);

    std::vector<ProcessingError> errors() const;

private:
    std::vector<ProcessingError> m_errors;
};

} // namespace nx::analytics::taxonomy
