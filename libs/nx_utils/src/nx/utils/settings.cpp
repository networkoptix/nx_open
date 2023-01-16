// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/utils/settings.h>

#include <QJsonArray>

#include <nx/utils/log/log.h>
#include <nx/utils/literal.h>

namespace nx::utils {

Settings::Settings()
{
}

Settings::~Settings()
{
}

void Settings::add(const QString& name, BaseOption* option)
{
    NX_ASSERT(m_options.find(name) == m_options.end(), lit("Duplicate setting: %1.").arg(name));
    m_options.emplace(name, option);
}

void Settings::attach(std::shared_ptr<AbstractQSettings>& settings)
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
            NX_WARNING(this, "Unknown option: %1", optionName);
            continue;
        }

        if (isOptionCommentedOut)
        {
            NX_INFO(this, "Option is commented out: %1", optionName);
            continue;
        }

        auto& option = optionIt->second;
        if (const auto setting = settings->value(optionName); !option->load(setting))
        {
            NX_ERROR(this, "Failed to load option: %1", optionName);
        }
        else
        {
            NX_DEBUG(
                this, "Loaded %1 loaded as '%2' from '%3'", optionName, option->save(), setting);
        }
    }

    applyMigrations(settings);
    m_loaded = true;
}

void Settings::sync()
{
    m_qtSettings->sync();
}

std::map<QString, SettingInfo> Settings::buildDocumentation() const
{
    std::map<QString, SettingInfo> documentation;
    for (const auto& option: m_options)
    {
        SettingInfo info;
        info.defaultValue = option.second->defaultValueVariant().toJsonValue();
        info.description = option.second->description();
        documentation.emplace(option.first, info);
    }
    return documentation;
}

} // namespace nx::utils
