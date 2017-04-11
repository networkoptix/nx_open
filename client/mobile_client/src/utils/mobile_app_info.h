#pragma once

#include <QtCore/QObject>

class QnMobileAppInfo : public QObject
{
    Q_OBJECT

public:
    explicit QnMobileAppInfo(QObject *parent = nullptr);

    Q_INVOKABLE QString productName() const;
    Q_INVOKABLE QString organizationName() const;
    Q_INVOKABLE QString version() const;
    Q_INVOKABLE QString cloudName() const;
    Q_INVOKABLE QString liteDeviceName() const;

    Q_INVOKABLE QUrl oldMobileClientUrl() const;
};
