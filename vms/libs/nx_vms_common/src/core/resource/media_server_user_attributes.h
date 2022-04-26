// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/singleton.h>

#include <nx/fusion/model_functions_fwd.h>

#include <core/resource/general_attribute_pool.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/media_server_data.h>

class NX_VMS_COMMON_API QnMediaServerUserAttributes
{
public:
    QnMediaServerUserAttributes(const nx::vms::api::MediaServerUserAttributesData& data);
    QnMediaServerUserAttributes() = default;
    void assign(const QnMediaServerUserAttributes& right, QSet<QByteArray>* const modifiedFields);
    QString name() const;
    void setName(const QString& name);
    int maxCameras() const;
    void setMaxCameras(int value);
    int locationId() const;
    void setLocationId(int value);
    QnUuid serverId() const;
    void setServerId(const QnUuid& serverId);
    bool isRedundancyEnabled() const;
    void setIsRedundancyEnabled(bool value);
    nx::vms::api::MediaServerUserAttributesData data() const;
    void setBackupBitrateBytesPerSecond(
        const nx::vms::api::BackupBitrateBytesPerSecond& backupBitrateBytesPerSecond);
    nx::vms::api::BackupBitrateBytesPerSecond backupBitrateBytesPerSecond() const;
    void setData(const nx::vms::api::MediaServerUserAttributesData& data);

private:
    nx::vms::api::MediaServerUserAttributesData m_data;
};

class NX_VMS_COMMON_API QnMediaServerUserAttributesPool:
    public QObject,
    public QnGeneralAttributePool<QnUuid, QnMediaServerUserAttributesPtr>
{
    Q_OBJECT
public:
    QnMediaServerUserAttributesPool(QObject *parent);
    QnMediaServerUserAttributesList getAttributesList( const QList<QnUuid>& idList );
};
