#pragma once

#include <map>
#include <functional>

#include <nx/utils/log/assert.h>

#include <QSettings>

namespace nx {
namespace utils {

/**
 * Container that offers declaration, access and changing of settings. Additionally, it
 * keeps description of options to build documentation. Options can be loaded or saved from
 * QSettings. To define a settings group, declare a struct derived from nx::utils::Settings. To
 * define an option, declare a struct member with type Option<>. To access an option value, use
 * operator().
 *
 * Usage example:
 * <pre><code>
 *
 *     struct Settings: nx::utils::Settings
 *     {
 *         Option<QString> option1{this, "option1", "defaultValue", "Option1 description"};
 *
 *         Option<int> option2{this, "option2",
 *             7, "Option2 description"};
 *
 *         // Overriding value accessor.
 *         Option<QString> option3{this, "option3", "", "Option3 description",
 *             [this](const QString& value)
 *             {
 *                 if (!value.isEmpty())
 *                     return value;
 *                 return option1();
 *             }
 *         };
 *     };
 *
 * </code></pre>
 */
class Settings
{
public:
    Settings() = default;
    bool load(const QSettings& settings);
    bool save(QSettings& settings) const;

protected:
    struct BaseOption
    {
        BaseOption(Settings* settings, const QString& name);

        bool present() const { return m_present; }
        bool removed() const { return m_removed; }

        virtual bool load(const QVariant& value) = 0;
        virtual QVariant save() const = 0;

        BaseOption(const BaseOption&) = delete;
        BaseOption& operator=(const BaseOption&) = delete;

    protected:
        bool m_present = false;
        bool m_removed = false;
    };

    template <typename T>
    struct Option final: BaseOption
    {
        using Accessor = std::function<T (const T&)>;
        Option(
            Settings* settings,
            const QString& name,
            const T& defaultValue,
            const QString& description,
            const Accessor& accessor = [](const T& value) { return value; })
            :
            BaseOption(settings, name),
            m_settings(settings),
            m_value(defaultValue),
            m_defaultValue(defaultValue),
            m_description(description),
            m_accessor(accessor)
        {
        }

        T operator()() const
        {
            NX_ASSERT(m_settings->m_loaded);
            return m_accessor(m_value);
        }

        T defaultValue() const { return m_defaultValue; }

        void set(const T& value)
        {
            m_value = value;
            m_present = true;
            m_removed = false;
        }

        void remove()
        {
            m_removed = true;
            m_present = false;
        }

    private:
        virtual bool load(const QVariant& value) override
        {
            if (!value.isValid() || !value.canConvert<T>())
                return false;

            m_value = value.value<T>();
            m_present = true;
            return true;
        }

        virtual QVariant save() const override
        {
            QVariant result;
            result.setValue(m_value);
            return result;
        }

    private:
        const Settings *const m_settings;
        T  m_value;
        T  m_defaultValue;
        QString m_description;
        Accessor m_accessor;
    };

    void add(const QString& name, BaseOption* option);

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

private:
    bool m_loaded = false;
    std::map<QString, BaseOption*> m_options;
};

} // namespace utils
} // namespace nx
