#include <nx/utils/settings.h>
#include <nx/utils/log/log.h>
#include <nx/utils/literal.h>

namespace nx {
namespace utils {

void Settings::add(const QString& name, BaseOption* option)
{
    NX_ASSERT(m_options.find(name) == m_options.end(), lit("Duplicate setting: %1.").arg(name));
    m_options.emplace(name, option);
}

bool Settings::load(const QSettings& settings)
{
    QStringList keys = settings.allKeys();
    for (const auto& key: keys)
    {
        auto optionIt = m_options.find(key);
        if (optionIt == m_options.end())
        {
            NX_LOG(lit("Unknown option: %1").arg(key), cl_logWARNING);
            continue;
        }
        if (!optionIt->second->load(settings.value(key)))
        {
            NX_LOG(lit("Failed to load option: %1").arg(key), cl_logERROR);
            return false;
        }
    }
    m_loaded = true;
    return true;
}

bool Settings::save(QSettings& settings) const
{
    for (auto& option: m_options)
    {
        if (option.second->present())
        {
            settings.setValue(option.first, option.second->save());
        }
        if (option.second->removed())
        {
            settings.remove(option.first);
        }
    }
    return true;
}

} // namespace utils
} // namespace nx
