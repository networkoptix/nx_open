#pragma once

#include <map>
#include <functional>
#include <chrono>

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
 *     class Settings: public nx::utils::Settings
 *     {
 *     public:
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
class NX_UTILS_API Settings
{
public:
    Settings() = default;
    bool load(const QSettings& settings);
    bool save(QSettings& settings) const;

protected:
    struct BaseOption
    {
        BaseOption(Settings* settings, const QString& name)
        {
            settings->add(name, this);
        }

        virtual ~BaseOption() = default;

        bool present() const { return isPresent; }
        bool removed() const { return isRemoved; }

        virtual bool load(const QVariant& value) = 0;
        virtual QVariant save() const = 0;

        BaseOption(const BaseOption&) = delete;
        BaseOption& operator=(const BaseOption&) = delete;

    protected:
        bool isPresent = false;
        bool isRemoved = false;
    };

    template<typename T>
    struct Option final: BaseOption
    {
        using Accessor = std::function<T(const T&)>;
        static inline const Accessor defaultAccessor = [](const T& value) { return value; };

        Option(
            Settings* settings,
            const QString& name,
            const T& defaultValue,
            const QString& description,
            const Accessor& accessor = defaultAccessor)
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
            isPresent = true;
            isRemoved = false;
        }

        void remove()
        {
            isRemoved = true;
            isPresent = false;
        }

    private:
        virtual bool load(const QVariant& value) override
        {
            if (!value.isValid() || !value.canConvert<T>())
                return false;

            m_value = value.value<T>();
            isPresent = true;
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

template<>
inline bool Settings::Option<std::chrono::milliseconds>::load(const QVariant& value)
{
    if (!value.isValid() || !value.canConvert<quint64>())
        return false;

    m_value = std::chrono::milliseconds(value.value<quint64>());
    isPresent = true;
    return true;
}

template<>
inline QVariant Settings::Option<std::chrono::milliseconds>::save() const
{
    QVariant result;
    result.setValue(m_value.count());
    return result;
}

} // namespace utils
} // namespace nx
