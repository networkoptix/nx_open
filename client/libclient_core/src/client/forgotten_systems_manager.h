#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <utils/common/connective.h>

typedef QSet<QString> QnForgottenSystemsSet;

class QnForgottenSystemsManager : public QObject, public Singleton<QnForgottenSystemsManager>
{
    Q_OBJECT
    typedef QObject base_type;


public:
    QnForgottenSystemsManager();

    ~QnForgottenSystemsManager() = default;

    void forgetSystem(const QString& id);

    bool isForgotten(const QString& id) const;

signals:
    void forgottenSystemsChanged();

private:
    void rememberSystem(const QString& id);

private:
    QnForgottenSystemsSet m_systems;
};

#define qnForgottenSystemsManager QnForgottenSystemsManager::instance()