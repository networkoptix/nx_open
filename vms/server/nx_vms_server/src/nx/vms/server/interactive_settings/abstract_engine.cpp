#include "abstract_engine.h"

#include <QtCore/QPointer>
#include <QtCore/QVector>
#include <QtCore/QSet>
#include <QtCore/QFile>

#include <nx/utils/log/log.h>

#include "components/value_item.h"
#include "components/group.h"
#include "components/section.h"

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

    if (const auto sections = qobject_cast<SectionContainer*>(item))
    {
        for (const auto section: sections->sectionList())
        {
            if (!processValueItemsRecursively(section, f))
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

bool AbstractEngine::loadModelFromFile(const QString& fileName)
{
    clearIssues();

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
    {
        addIssue(Issue(Issue::Type::error, Issue::Code::ioError,
            lm("Cannot open file %1.").arg(fileName)));
        return false;
    }

    constexpr qint64 kMaxFileSize = 1024 * 1024;
    if (file.size() > kMaxFileSize)
    {
        addIssue(Issue(Issue::Type::error, Issue::Code::ioError,
           lm("Cannot load file %1. File is too large: %2 bytes (max: %3 bytes).").args(
               fileName, file.size(), kMaxFileSize)));
        return false;
    }

    const QByteArray data = file.readAll();

    file.close();

    return loadModelFromData(data);
}

Settings* AbstractEngine::settingsItem() const
{
    return m_settingsItem.get();
}

bool AbstractEngine::setSettingsItem(std::unique_ptr<Item> item)
{
    if (!item)
    {
        m_settingsItem.reset();
        return true;
    }

    const auto settings = qobject_cast<components::Settings*>(item.get());
    if (!settings)
    {
        addIssue(Issue(Issue::Type::error, Issue::Code::parseError,
            lm("Root item must have \"Settings\" type but \"%1\" is provided.")
                .arg(item->type())));
        return false;
    }

    m_settingsItem.reset(settings);
    item.release();

    QString nonUniqueName;
    bool valid = processValueItemsRecursively(settings,
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
        addIssue(Issue(Issue::Type::error, Issue::Code::itemNameIsNotUnique,
            lm("Item name is not unique: %1").arg(nonUniqueName)));
        return false;
    }

    return true;
}

void AbstractEngine::initValues()
{
    startUpdatingValues();

    processValueItemsRecursively(m_settingsItem.get(),
        [](ValueItem* item)
        {
            if (item->defaultJsonValue().isUndefined())
            {
                item->setDefaultJsonValue(item->fallbackDefaultValue());
            }
            else
            {
                // Since we're inside updatingValues() we have to normalize value manually.
                const auto& normalized = item->normalizedValue(item->defaultJsonValue());
                item->setDefaultJsonValue(
                    normalized.isUndefined() ? item->fallbackDefaultValue() : normalized);
            }

            item->setJsonValue(item->defaultJsonValue());
            return true;
        });

    stopUpdatingValues();

    processValueItemsRecursively(m_settingsItem.get(),
        [](ValueItem* item)
        {
            item->applyConstraints();
            return true;
        });
}

QJsonObject AbstractEngine::serializeModel() const
{
    if (const auto item = settingsItem())
        return item->serializeModel();

    return {};
}

QJsonObject AbstractEngine::values() const
{
    if (!m_settingsItem)
        return {};

    QJsonObject result;
    processValueItemsRecursively(m_settingsItem.get(),
        [&result](ValueItem* item)
        {
            result[item->name()] = item->jsonValue();
            return true;
        });

    return result;
}

void AbstractEngine::applyValues(const QJsonObject& values)
{
    clearIssues();

    if (!m_settingsItem)
        return;

    startUpdatingValues();

    processValueItemsRecursively(m_settingsItem.get(),
        [&values](ValueItem* item)
        {
            const auto it = values.find(item->name());
            if (it == values.end())
                return true;

            item->setJsonValue(it.value());
            return true;
        });

    stopUpdatingValues();

    processValueItemsRecursively(m_settingsItem.get(),
        [](ValueItem* item)
        {
            item->applyConstraints();
            return true;
    });
}

void AbstractEngine::applyStringValues(const QJsonObject& values)
{
    m_skipStringConversionWarnings = true;
    applyValues(values);
    m_skipStringConversionWarnings = false;
}

AbstractEngine::ModelAndValues AbstractEngine::tryValues(const QJsonObject& values)
{
    clearIssues();

    const auto originalValues = this->values();

    applyValues(values);

    const auto issues = m_issues;

    const auto newModel = serializeModel();
    const auto newValues = this->values();

    applyValues(originalValues);

    // We don't need issues from the rollback procedure.
    m_issues = issues;

    return {newModel, newValues};
}

void AbstractEngine::clearIssues()
{
    m_issues.clear();
}

void AbstractEngine::addIssue(const Issue& issue)
{
    m_issues.append(issue);
    NX_DEBUG(this, "%1: %2", issue.code, issue.message);
}

QList<Issue> AbstractEngine::issues() const
{
    return m_issues;
}

bool AbstractEngine::hasErrors() const
{
    return std::any_of(m_issues.begin(), m_issues.end(),
        [](const Issue& issue){ return issue.type == Issue::Type::error; });
}

} // namespace nx::vms::server::interactive_settings
