#include "abstract_engine.h"

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QSet>
#include <QtCore/QFile>

#include <nx/utils/log/log_message.h>

#include "components/items.h"

namespace nx::vms::server::interactive_settings {

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

AbstractEngine::AbstractEngine()
{
}

AbstractEngine::~AbstractEngine()
{
}

AbstractEngine::Error AbstractEngine::loadModelFromFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return Error(ErrorCode::cannotOpenFile, lm("Cannot open file %1.").arg(fileName));

    constexpr qint64 kMaxFileSize = 1024 * 1024;
    if (file.size() > kMaxFileSize)
    {
        return Error(
            ErrorCode::fileIsTooLarge,
            lm("Cannot load file %1. File is too large: %2 bytes (max: %3 bytes).").args(
                fileName, file.size(), kMaxFileSize));
    }

    const QByteArray data = file.readAll();

    file.close();

    return loadModelFromData(data);
}

Settings* AbstractEngine::settingsItem() const
{
    return m_settingsItem;
}

AbstractEngine::Error AbstractEngine::setSettingsItem(Settings* item)
{
    m_settingsItem = item;
    if (!item)
        return ErrorCode::ok;

    QString nonUniqueName;
    bool valid = processValueItemsRecursively(item,
        [names = QSet<QString>(), &nonUniqueName](ValueItem* item) mutable
        {
            const auto name = item->name();

            if (names.contains(name))
            {
                nonUniqueName = name;
                return false;
            }

            names.insert(name);
            return true;
        });

    if (!valid)
    {
        return Error(ErrorCode::itemNameIsNotUnique,
            lm("Item name is not unique: %1").arg(nonUniqueName));
    }

    return ErrorCode::ok;
}

QJsonObject AbstractEngine::serializeModel() const
{
    if (const auto item = settingsItem())
        return item->serialize();

    return {};
}

QVariantMap AbstractEngine::values() const
{
    if (!m_settingsItem)
        return {};

    QVariantMap result;
    processValueItemsRecursively(m_settingsItem,
        [&result](ValueItem* item)
        {
            result[item->name()] = item->value();
            return true;
        });

    return result;
}

void AbstractEngine::applyValues(const QVariantMap& values) const
{
    if (!m_settingsItem)
        return;

    processValueItemsRecursively(m_settingsItem,
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

AbstractEngine::ModelAndValues AbstractEngine::tryValues(const QVariantMap& values) const
{
    const auto originalValues = this->values();

    applyValues(values);

    const auto newModel = serializeModel();
    const auto newValues = this->values();

    applyValues(originalValues);

    return {newModel, newValues};
}

} // namespace nx::vms::server::interactive_settings
