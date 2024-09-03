// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::branding {

class NX_BRANDING_API QmlProxy: public QObject
{
    Q_OBJECT

public:
    QmlProxy(QObject* parent = nullptr);

    Q_INVOKABLE static QString cloudName();
    Q_INVOKABLE static QString company();
    Q_INVOKABLE static QString companyUrl();
    Q_INVOKABLE static QString vmsName();
    Q_INVOKABLE static bool isDesktopClientCustomized();
    Q_INVOKABLE static QString supportAddress();
    Q_INVOKABLE static QString defaultLocale();
};

} // namespace nx::branding
