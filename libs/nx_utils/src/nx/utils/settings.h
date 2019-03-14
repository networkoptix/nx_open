#pragma once

#include <map>
#include <functional>
#include <chrono>

#include <nx/utils/log/assert.h>

#include <QSettings>
#include <QJsonObject>


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
    void attach(const std::shared_ptr<QSettings>& settings);

    QJsonObject buildDocumentation() const;

protected:
    struct BaseOption
    {
        BaseOption(Settings* settings, const QString& name, QString description):
            m_description(std::move(description))
        {
            settings->add(name, this);
        }

        virtual ~BaseOption() = default;

        bool present() const { return isPresent; }
        const QString& description() { return m_description; }

        virtual bool load(const QVariant& value) = 0;
        virtual QVariant save() const = 0;
        virtual QVariant defaultValueVariant() const = 0;

        BaseOption(const BaseOption&) = delete;
        BaseOption& operator=(const BaseOption&) = delete;

    protected:
        QString m_description;
        bool isPresent = false;
    };

    template<typename T>
    struct Option final: BaseOption
    {
        using Accessor = std::function<T(const T&)>;
        static inline const Accessor defaultAccessor = [](const T& value) { return value; };

        Option(
            Settings* settings,
            const QString& name,
            T defaultValue,
            QString description,
            Accessor accessor = defaultAccessor)
            :
            BaseOption(settings, name, std::move(description)),
            m_settings(settings),
            m_value(defaultValue),
            m_defaultValue(std::move(defaultValue)),
            m_name(name),
            m_accessor(std::move(accessor))
        {
        }

        T operator()() const
        {
            NX_ASSERT(m_settings->m_loaded);
            return m_accessor(m_value);
        }

        T defaultValue() const { return m_defaultValue; }

        void set(T value)
        {
            m_value = std::move(value);
            isPresent = true;
            m_settings->m_qtSettings->setValue(m_name, save());
        }

        void remove()
        {
            isPresent = false;
            m_settings->m_qtSettings->remove(m_name);
        }

    private:
        virtual bool load(const QVariant& value) override
        {
            if (!fromQVariant(value, &m_value))
                return false;

            isPresent = true;
            return true;
        }

        virtual QVariant save() const override
        {
            return toQVariant(m_value);
        }

        virtual QVariant defaultValueVariant() const override
        {
            return toQVariant(m_defaultValue);
        }

        static bool fromQVariant(const QVariant& qVariant, T* outValue)
        {
            if (!qVariant.isValid() || !qVariant.canConvert<T>())
                return false;

            *outValue = qVariant.value<T>();
            return true;
        }

        static QVariant toQVariant(const T& value)
        {
            QVariant result;
            result.setValue(value);
            return result;
        }

    private:
        const Settings *const m_settings;
        T  m_value;
        T  m_defaultValue;
        QString m_name;

        Accessor m_accessor;
    };

    void add(const QString& name, BaseOption* option);

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

private:
    bool m_loaded = false;
    std::map<QString, BaseOption*> m_options;
    std::shared_ptr<QSettings> m_qtSettings;
};


// Override string loader to workaround StringList parsing when input string contains comma
template<>
inline bool Settings::Option<QString>::fromQVariant(const QVariant& value, QString* result)
{
    if (!value.isValid())
        return false;

    *result = value.toString();
    return true;
}

template<>
inline bool Settings::Option<std::chrono::milliseconds>::fromQVariant(
    const QVariant& value, std::chrono::milliseconds* result)
{
    if (!value.isValid() || !value.canConvert<quint64>())
        return false;

    *result = std::chrono::milliseconds(value.value<quint64>());
    return true;
}

template<>
inline QVariant Settings::Option<std::chrono::milliseconds>::toQVariant(
    const std::chrono::milliseconds& value)
{
    QVariant result;
    result.setValue(value.count());
    return result;
}

} // namespace utils
} // namespace nx

Q_DECLARE_METATYPE(std::chrono::seconds);