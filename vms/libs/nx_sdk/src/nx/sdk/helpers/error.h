// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_error.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/string_map.h>

namespace nx {
namespace sdk {

class Error: public RefCountable<IError>
{
public:
    Error();
    Error(ErrorCode errorCode, const char* message);
    virtual void setError(ErrorCode errorCode, const char* message) override;
    virtual void setDetail(const char* key, const char* message) override;

    virtual ErrorCode errorCode() const override;
    virtual const IString* message() const override;
    virtual const IStringMap* details() const override;

private:
    ErrorCode m_errorCode = ErrorCode::noError;
    Ptr<String> m_message;
    Ptr<StringMap> m_details;
};

} // namespace sdk
} // namespace nx
