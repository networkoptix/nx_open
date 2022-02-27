// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource.h>
#include <QtCore/QMetaObject>

class QnCommonModule;

namespace nx::core::resource {

template<typename BaseResource>
class ResourceWithLocalPropertyStorage: public BaseResource
{
public:
    ResourceWithLocalPropertyStorage(QnCommonModule* commonModule): BaseResource(commonModule) {};

    virtual bool hasProperty(const QString& key) const override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_properties.find(key) != m_properties.cend();
    }

    virtual QString getProperty(const QString& key) const override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (auto it = m_properties.find(key); it != m_properties.cend())
            return it->second;

        return QString();
    }

    virtual bool setProperty(
        const QString& key,
        const QString& value,
        QnResource::PropertyOptions /*options*/) override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_propertiesPrevValues[key] = m_properties[key];
        m_properties[key] = value;
        m_changedProperties.insert(key);
        return true;
    }

    virtual bool setProperty(
        const QString& key,
        const QVariant& value,
        QnResource::PropertyOptions /*options*/) override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_propertiesPrevValues[key] = m_properties[key];
        m_properties[key] = value.toString();
        m_changedProperties.insert(key);
        return true;
    }

    virtual bool saveProperties() override
    {
        std::set<QString> changedProperties;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            changedProperties = std::move(m_changedProperties);
            m_changedProperties.clear();
        }

        for (const auto& changedProperty: changedProperties)
        {
            BaseResource::propertyChanged(
                toSharedPointer(this),
                changedProperty,
                m_propertiesPrevValues[changedProperty],
                m_properties[changedProperty]);
        }

        return true;
    };

    virtual int savePropertiesAsync() override
    {
        return /*Fake handle*/ 1;
    }

private:
    mutable nx::Mutex m_mutex;
    std::map<QString, QString> m_properties;
    std::map<QString, QString> m_propertiesPrevValues;
    mutable std::set<QString> m_changedProperties;
};

} // namespace nx::core::resource
