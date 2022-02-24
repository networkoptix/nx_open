// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "error_handler.h"

namespace nx::analytics::taxonomy {

void ErrorHandler::handleError(ProcessingError error)
{
    m_errors.push_back(std::move(error));
}

std::vector<ProcessingError> ErrorHandler::errors() const
{
    return m_errors;
}

} // namespace nx::analytics::taxonomy
