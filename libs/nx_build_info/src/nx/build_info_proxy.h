// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::build_info {

class NX_BUILD_INFO_API QmlProxy: public QObject
{
    Q_OBJECT

public:
    QmlProxy(QObject* parent = nullptr);

    Q_INVOKABLE static QString vmsVersion();
};

} // namespace nx::build_info
