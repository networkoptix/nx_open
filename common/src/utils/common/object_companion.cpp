#include "object_companion.h"

QObject* QnObjectCompanionManager::companion(QObject* parent, const char* id)
{
    return getCompanion(parent, companionId(id));
}

QObject* QnObjectCompanionManager::detach(QObject* parent, const char* id)
{
    const QByteArray internalId = companionId(id);
    QObject* result = getCompanion(parent, internalId);
    clearCompanion(parent, internalId);
    return result;
}

bool QnObjectCompanionManager::uninstall(QObject* parent, const char* id)
{
    QScopedPointer<QObject> deleter(detach(parent, id));
    return !deleter.isNull();
}

bool QnObjectCompanionManager::attach(QObject* parent, QObject* companion, const char* id)
{
    const QByteArray internalId = companionId(id);
    if (getCompanion(parent, internalId))
        return false;

    setCompanion(parent, internalId, companion);
    return true;
}

void QnObjectCompanionManager::attach(QObject* parent, QObject* companion, const char* id, QObject*& previousCompanion)
{
    const QByteArray internalId = companionId(id);
    previousCompanion = getCompanion(parent, internalId);
    setCompanion(parent, internalId, companion);
}

QObject* QnObjectCompanionManager::getCompanion(QObject* parent, const QByteArray& internalId)
{
    return parent->property(internalId).value<QObject*>();
}

void QnObjectCompanionManager::setCompanion(QObject* parent, const QByteArray& internalId, QObject* companion)
{
    parent->setProperty(internalId, QVariant::fromValue(companion));
}

void QnObjectCompanionManager::clearCompanion(QObject* parent, const QByteArray& internalId)
{
    parent->setProperty(internalId, QVariant());
}

QByteArray QnObjectCompanionManager::companionId(const char* id)
{
    static const QByteArray kPrefix("_qn_companion_");
    return kPrefix + id;
}

