// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <network/base_system_description.h>

class NX_VMS_CLIENT_CORE_API QnAbstractSystemsFinder: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnAbstractSystemsFinder(QObject *parent = nullptr);

    virtual ~QnAbstractSystemsFinder();

    typedef QList<QnSystemDescriptionPtr> SystemDescriptionList;
    virtual SystemDescriptionList systems() const = 0;

    virtual QnSystemDescriptionPtr getSystem(const QString &id) const = 0;

signals:
    void systemDiscovered(const QnSystemDescriptionPtr &system);

    void systemLost(const QString &id);

    void systemLostInternal(const QString &id, const QnUuid& localId);
};
