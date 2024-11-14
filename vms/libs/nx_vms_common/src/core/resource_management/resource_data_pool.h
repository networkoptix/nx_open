// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <core/resource/resource_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/software_version.h>
#include <nx/utils/thread/mutex.h>

class NX_VMS_COMMON_API QnResourceDataPool: public QObject
{
    Q_OBJECT;
public:
    QnResourceDataPool(QObject *parent = NULL);
    virtual ~QnResourceDataPool();

    /**
     * \param camera                    resource to get data for.
     * \returns                         Resource data for the given camera.
     */
    template<typename DeviceType>
    QnResourceData data(const QnSharedResourcePointer<DeviceType>& camera) const
    {
        if (!camera)
            return QnResourceData();

        if (m_externalSource)
            return m_externalSource->data(camera);

        return data(camera->getVendor(), camera->getModel(), camera->getFirmware());
    }

    QnResourceData data(
        const QString& _vendor,
        const QString& model,
        const QString& firmware = {}) const;

    /**
     * Resource Data pools in the Cross-System contexts may use shared data source to avoid
     * excessive data loading.
     */
    void setExternalSource(QnResourceDataPool* source);

    static nx::utils::SoftwareVersion getVersion(const QByteArray& data);
    static bool validateData(const QByteArray& data);

    bool loadFile(const QString& fileName);
    bool loadData(const QByteArray& data);
    void clear();
    QJsonObject allData() const;

signals:
    void changed();

private:
    struct Key
    {
        QString vendor;
        QString model;
        QString firmware;

        Key(const QString& _vendor, const QString& _model, const QString& _firmware = {});
        static Key fromString(const QString& key); //< string format: VENDOR|MODEL|FIRMWARE

        bool matches(const Key& other) const;
        auto toComparable() const noexcept
        {
            return std::tie(vendor, model, firmware);
        }

        bool operator==(const Key& other) const;
        bool operator<(const Key& other) const;

        friend size_t qHash(const Key& key)
        {
            return qHash(key.vendor) ^ qHash(key.model) ^ qHash(key.firmware);
        }
    };

private:
    bool loadInternal(const QString& fileName);
    QnResourceData data(const Key& key) const;

    void setData(QJsonObject allData, std::vector<std::pair<Key, QnResourceData>> dataByKey);

private:
    mutable nx::Mutex m_mutex;
    QJsonObject m_allData;
    std::vector<std::pair<Key, QnResourceData>> m_dataByKey;

    /** Cache of the search results to avoid using too much regexps. */
    mutable QHash<Key, QnResourceData> m_cachedResultByKey;

    QPointer<QnResourceDataPool> m_externalSource;
};
