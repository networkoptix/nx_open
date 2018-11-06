#include "object_companion.h"

#include <QtCore/QPointer>

namespace nx::vms::client::desktop {

namespace {

static QObject* getCompanion(QObject* parent, const QByteArray& internalId)
{
    return parent->property(internalId).value<QPointer<QObject>>();
}

static void setCompanion(QObject* parent, const QByteArray& internalId, QPointer<QObject> companion)
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

QObject* ObjectCompanionManager::companion(QObject* parent, const char* id)
{
    return getCompanion(parent, companionId(id));
}

std::unique_ptr<QObject> ObjectCompanionManager::detach(QObject* parent, const char* id)
{
    const QByteArray internalId = companionId(id);
    std::unique_ptr<QObject> result(getCompanion(parent, internalId));
    if (!result)
        return nullptr;
    result->setParent(nullptr);
    clearCompanion(parent, internalId);
    return result;
}

bool ObjectCompanionManager::uninstall(QObject* parent, const char* id)
{
    return detach(parent, id) != nullptr;
}

std::unique_ptr<QObject> ObjectCompanionManager::attach(
    QObject* parent, QObject* companion, const char* id)
{
    const QByteArray internalId = companionId(id);
    std::unique_ptr<QObject> previousCompanion(getCompanion(parent, internalId));
    setCompanion(parent, internalId, companion);
    companion->setParent(parent);
    return previousCompanion;
}

} // namespace nx::vms::client::desktop
