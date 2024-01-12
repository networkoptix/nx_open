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

    /**
     * Some operations return an error code which shows the status of the execution. The user
     * should not rely on numeric values but rather should use the following defined codes.
     *
     * Example:
     *
     *     const error = await vms.tab.saveLayout()
     *     if (error.code != vms.ErrorCode.success)
     *         alert(`Can't save layout: ${error.description}`)
     *
     * @ingroup vms
     */
    enum ErrorCode
    {
        /** The operation was successful. */
        success,

        /** The operation has failed because of an unspecified error. */
        failed,

        /** No access rights to preform an operation. */
        denied,

        /** Invalid arguments specified. */
        invalid_args,
    };
    Q_ENUMS(ErrorCode)
};

} // namespace nx::vms::client::desktop::jsapi
