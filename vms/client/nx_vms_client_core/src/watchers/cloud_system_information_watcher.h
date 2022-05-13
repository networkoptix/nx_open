// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

class QnCloudSystemInformationWatcherPrivate;

class NX_VMS_CLIENT_CORE_API QnCloudSystemInformationWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString systemId READ systemId NOTIFY systemIdChanged)
    Q_PROPERTY(QString ownerEmail READ ownerEmail NOTIFY ownerEmailChanged)
    Q_PROPERTY(QString ownerFullName READ ownerFullName NOTIFY ownerFullNameChanged)
    Q_PROPERTY(QString ownerDescription READ ownerDescription NOTIFY ownerDescriptionChanged)

    using base_type = QObject;

public:
    QnCloudSystemInformationWatcher(QObject* partent = nullptr);
    ~QnCloudSystemInformationWatcher();

    QString systemId() const;
    QString ownerEmail() const;
    QString ownerFullName() const;
    QString ownerDescription() const;

signals:
    void systemIdChanged();
    void ownerEmailChanged();
    void ownerFullNameChanged();
    void ownerDescriptionChanged();

private:
    QScopedPointer<QnCloudSystemInformationWatcherPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudSystemInformationWatcher)
};
