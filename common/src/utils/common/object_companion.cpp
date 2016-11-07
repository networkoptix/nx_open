#include "object_companion.h"

namespace {

static QObject* getCompanion(QObject* parent, const QByteArray& internalId)
{
    return parent->property(internalId).value<QObject*>();
}

static void setCompanion(QObject* parent, const QByteArray& internalId, QObject* companion)
{
    parent->setProperty(internalId, QVariant::fromValue(companion));
}

static void clearCompanion(QObject* parent, const QByteArray& internalId)
{
    parent->setProperty(internalId, QVariant());
}

static QByteArray companionId(const char* id)
{
    static const QByteArray kPrefix("_qn_companion_");
    return kPrefix + id;
}

} // namespace

QObject* QnObjectCompanionManager::companion(QObject* parent, const char* id)
{
    return getCompanion(parent, companionId(id));
}

std::unique_ptr<QObject> QnObjectCompanionManager::detach(QObject* parent, const char* id)
{
    const QByteArray internalId = companionId(id);
    std::unique_ptr<QObject> result(getCompanion(parent, internalId));
    clearCompanion(parent, internalId);
    return result;
}

bool QnObjectCompanionManager::uninstall(QObject* parent, const char* id)
{
    return detach(parent, id) != nullptr;
}

std::unique_ptr<QObject> QnObjectCompanionManager::attach(QObject* parent, QObject* companion, const char* id)
{
    const QByteArray internalId = companionId(id);
    std::unique_ptr<QObject> previousCompanion(getCompanion(parent, internalId));
    setCompanion(parent, internalId, companion);
    return previousCompanion;
}
