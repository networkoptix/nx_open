// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class OpenGLContextLogger: public QObject
{
public:
    explicit OpenGLContextLogger(QObject* parent = nullptr);
    virtual ~OpenGLContextLogger();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
