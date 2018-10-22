#include "abstract_engine.h"

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QSet>

#include "components/items.h"

namespace nx::mediaserver::interactive_settings {

using namespace components;

namespace {

bool processValueItemsRecursively(Item* item, const std::function<bool(ValueItem*)>& f)
{
    if (const auto valueItem = qobject_cast<ValueItem*>(item))
        return f(valueItem);

    if (const auto group = qobject_cast<Group*>(item))
    {
        for (const auto item: group->itemsList())
        {
            if (!processValueItemsRecursively(item, f))
                return false;
        }
    }

    return true;
}

} // namespace

class AbstractEngine::Private
{
public:
    QPointer<Settings> settingsItem;
    AbstractEngine::Status status = AbstractEngine::Status::idle;
};

AbstractEngine::AbstractEngine(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

AbstractEngine::~AbstractEngine() = default;

AbstractEngine::Status AbstractEngine::status() const
{
    return d->status;
}

QObject* AbstractEngine::rootObject() const
{
    return d->settingsItem;
}

void AbstractEngine::setStatus(AbstractEngine::Status status)
{
    if (d->status == status)
        return;

    d->status = status;
}

Settings* AbstractEngine::settingsItem() const
{
    return d->settingsItem;
}

void AbstractEngine::setSettingsItem(Settings* item)
{
    d->settingsItem = item;
    if (!item)
        return;

    bool valid = processValueItemsRecursively(item,
        [names = QSet<QString>()](ValueItem* item) mutable
        {
            if (names.contains(item->name()))
                return false;

            names.insert(item->name());
            return true;
        });

    if (!valid)
    {
        d->settingsItem = nullptr;
        setStatus(Status::error);
        return;
    }

    setStatus(Status::loaded);
}

QJsonObject AbstractEngine::serialize() const
{
    if (const auto item = settingsItem())
        return item->serialize();

    return {};
}

QVariantMap AbstractEngine::values() const
{
    if (!d->settingsItem)
        return {};

    QVariantMap result;
    processValueItemsRecursively(d->settingsItem,
        [&result](ValueItem* item)
        {
            result[item->name()] = item->value();
            return true;
        });

    return result;
}

void AbstractEngine::applyValues(const QVariantMap& values) const
{
    if (!d->settingsItem)
        return;

    processValueItemsRecursively(d->settingsItem,
        [&values](ValueItem* item)
        {
            const auto it = values.find(item->name());
            if (it == values.end())
                return true;

            item->setValue(*it);
            return true;
        });

    return;
}

QJsonObject AbstractEngine::tryValues(const QVariantMap& values) const
{
    const auto originalValues = this->values();

    applyValues(values);

    const auto result = serialize();

    applyValues(originalValues);

    return result;
}

} // namespace nx::mediaserver::interactive_settings
