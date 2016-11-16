#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <client_core/client_core_meta_types.h>

#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <utils/common/connective.h>

class QnForgottenSystemsManager: public QObject, public Singleton<QnForgottenSystemsManager>
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnForgottenSystemsManager();

    ~QnForgottenSystemsManager() = default;

    void forgetSystem(const QString& id);

    bool isForgotten(const QString& id) const;

signals:
    void forgottenSystemAdded(const QString& id);

    void forgottenSystemRemoved(const QString& id);

private:
    void rememberSystem(const QString& id);

private:
    QnStringSet m_systems;
};

#define qnForgottenSystemsManager QnForgottenSystemsManager::instance()