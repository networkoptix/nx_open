// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>

class QnMobileAppInfo : public QObject
{
    Q_OBJECT

public:
    explicit QnMobileAppInfo(QObject *parent = nullptr);

    Q_INVOKABLE QString productName() const;
    Q_INVOKABLE QString version() const;
    Q_INVOKABLE QString cloudName() const;
    Q_INVOKABLE QString shortCloudName() const;
};
