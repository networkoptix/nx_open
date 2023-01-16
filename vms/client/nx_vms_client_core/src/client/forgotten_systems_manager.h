// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QSet>

#include <client_core/client_core_meta_types.h>
#include <nx/utils/uuid.h>

class NX_VMS_CLIENT_CORE_API QnForgottenSystemsManager: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnForgottenSystemsManager();

    ~QnForgottenSystemsManager() = default;

    void forgetSystem(const QString& id);

    bool isForgotten(const QString& id) const;

    QStringList forgottenSystems() const;

signals:
    void forgottenSystemAdded(const QString& id);

    void forgottenSystemRemoved(const QString& id);

private:
    void rememberSystem(const QString& id);

private:
    QnStringSet m_systems;
};
