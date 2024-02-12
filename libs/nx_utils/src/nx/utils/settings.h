// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QJsonObject>
#include <QSettings>
#include <chrono>
#include <functional>
#include <map>

#include <nx/reflect/instrument.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

namespace nx::utils {

namespace detail {

template<typename Rep, typename Period>
bool fromQVariant(const QVariant& value, std::chrono::duration<Rep, Period>* result)
{
    if (!value.isValid() || !value.canConvert<quint64>())
        return false;

    *result = std::chrono::duration<Rep, Period>(value.value<quint64>());
    return true;
}

// Override string loader to workaround StringList parsing when input string contains comma
inline bool fromQVariant(const QVariant& value, QString* result)
{
    if (!value.isValid())
        return false;
    if (value.typeId() == QMetaType::QStringList)
        *result = value.toStringList().join(",");
    else
        *result = value.toString();
    return true;
}

inline bool fromQVariant(const QVariant& value, nx::Uuid* result)
{
    if (!value.isValid())
        return false;

    *result = nx::Uuid::fromStringSafe(value.toString());
    return true;
}

inline QVariant toQVariant(const nx::Uuid& value)
{
    return QVariant::fromValue(value.toString());
}

template<typename T>
bool fromQVariant(const QVariant& qVariant, T* outValue)
{
    if (!qVariant.isValid() || !qVariant.canConvert<T>())
        return false;

    *outValue = qVariant.value<T>();
    return true;
}

template<typename Rep, typename Period>
QVariant toQVariant(const std::chrono::duration<Rep, Period>& value)
{
    QVariant result;
    result.setValue(value.count());
    return result;
}

template<typename T>
QVariant toQVariant(const T& value)
{
    QVariant result;
    result.setValue(value);
    return result;
}

} // namespace detail

struct NX_UTILS_API SettingInfo
{
    QJsonValue defaultValue;
    QString description;
};
#define SettingInfo_Fields (defaultValue)(description)

class AbstractQSettings
{
public:
    virtual ~AbstractQSettings() = default;
    virtual void setValue(const QString& name, const QVariant& value) = 0;
    virtual void remove(const QString& name) = 0;
    virtual QStringList allKeys() const = 0;
    virtual QVariant value(const QString& name) const = 0;
    virtual bool contains(const QString& name) = 0;
    virtual void sync() = 0;
    virtual void beginGroup(const QString& name) = 0;
    virtual void endGroup() = 0;
    virtual QSettings* qSettings() = 0;
};

/**
 * Container that offers declaration, access and changing of settings. Additionally, it
 * keeps description of options to build documentation. Options can be loaded or saved from
 * QSettings. To define a settings group, declare a struct derived from nx::utils::Settings. To
 * define an option, declare a struct member with type Option<>. To access an option value, use
 * operator().
 *
 * ATTENTION: Values in the .conf file can be enquoted and then can include a semicolon, otherwise
 * the semicolon is treated as a comment start. The value is then returned without the quotes.
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
    Settings();
    virtual ~Settings();

    void attach(std::shared_ptr<AbstractQSettings>& settings);
    void sync();

    std::map<QString, SettingInfo> buildDocumentation() const;

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
            NX_VERBOSE(this, "Set %1 to '%2'", m_name, value);
            m_value = std::move(value);
            isPresent = true;
            m_settings->m_qtSettings->setValue(m_name, save());
        }

        void remove()
        {
            isPresent = false;
            m_settings->m_qtSettings->remove(m_name);
        }

        QString name() const
        {
            return m_name;
        }

    private:
        virtual bool load(const QVariant& value) override
        {
            if (!detail::fromQVariant(value, &m_value))
            {
                NX_WARNING(this, "%1: Failed %2 loading from '%3'", __func__, m_name, value);
                return false;
            }

            NX_VERBOSE(this, "Loaded %1 as '%2' from %3", m_name, save(), value);
            isPresent = true;
            return true;
        }

        virtual QVariant save() const override
        {
            return detail::toQVariant(m_value);
        }

        virtual QVariant defaultValueVariant() const override
        {
            return detail::toQVariant(m_defaultValue);
        }

    private:
        const Settings* const m_settings;
        T m_value;
        T m_defaultValue;
        QString m_name;

        Accessor m_accessor;
    };

    void add(const QString& name, BaseOption* option);

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

private:
    bool m_loaded = false;
    std::map<QString, BaseOption*> m_options;
    std::shared_ptr<AbstractQSettings> m_qtSettings;

    virtual void applyMigrations(std::shared_ptr<AbstractQSettings>& settings) = 0;
};

} // namespace nx::utils
