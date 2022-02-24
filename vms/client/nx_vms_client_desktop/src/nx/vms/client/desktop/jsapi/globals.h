// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::desktop::jsapi {

/** Class contains global constants which may be used in the JS scripts. */
class Globals: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    enum ErrorCode
    {
        success, //< Success operation status.
        failed, //< Operation is failed because of unspecified error.
        denied, //< No accesss rights to preform an operation.
        invalid_args //< Invalid arguments specified.
    };
    Q_ENUMS(ErrorCode)
};

} // namespace nx::vms::client::desktop::jsapi
