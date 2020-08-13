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
        // "_" in the beginning of the key means that the option is commented out.
        bool isOptionCommentedOut = key.startsWith('_');
        QString optionName = isOptionCommentedOut ? key.mid(1) : key;

        auto optionIt = m_options.find(optionName);
        if (optionIt == m_options.end())
        {
            NX_WARNING(this, lit("Unknown option: %1").arg(optionName));
            continue;
        }

        if (isOptionCommentedOut)
        {
            NX_INFO(this, lit("Option is commented out: %1").arg(optionName));
            continue;
        }

        if (!optionIt->second->load(settings->value(optionName)))
            NX_ERROR(this, lit("Failed to load option: %1").arg(optionName));
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
