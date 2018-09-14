#include <nx/utils/settings.h>

#include <QJsonArray>

#include <nx/utils/log/log.h>
#include <nx/utils/literal.h>



namespace nx {
namespace utils {

void Settings::add(const QString& name, BaseOption* option)
{
    NX_ASSERT(m_options.find(name) == m_options.end(), lit("Duplicate setting: %1.").arg(name));
    m_options.emplace(name, option);
}

void Settings::attach(const std::shared_ptr<QSettings>& settings)
{
    m_qtSettings = settings;
    QStringList keys = settings->allKeys();
    for (const auto& key: keys)
    {
        auto optionIt = m_options.find(key);
        if (optionIt == m_options.end())
        {
            NX_WARNING(this, lit("Unknown option: %1").arg(key));
            continue;
        }
        if (!optionIt->second->load(settings->value(key)))
            NX_ERROR(this, lit("Failed to load option: %1").arg(key));
    }
    m_loaded = true;
}

QJsonObject Settings::buildDocumentation() const
{
    QJsonObject documentation;
    QJsonArray settings;
    for (const auto& option: m_options)
    {
        QJsonObject description;
        description.insert("name", option.first);
        description.insert("defaultValue", option.second->defaultValueVariant().toString());
        description.insert("description", option.second->description());
        settings << description;
    }
    documentation.insert("settings", settings);
    return documentation;
}

} // namespace utils
} // namespace nx
