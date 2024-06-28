// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core::testkit {

class NX_VMS_CLIENT_CORE_API HttpServer: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    HttpServer(int port);
    virtual ~HttpServer();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core::testkit
