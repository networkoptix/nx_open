// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/device_search.h>

typedef nx::vms::api::DeviceSearchStatus QnManualResourceSearchStatus;

// TODO: #virtualCamera better split this struct in two: name, vendor & existsInPool are unused
// in add requests.
struct QnManualResourceSearchEntry
{
    QString name;
    QString model;
    QString url;
    QString manufacturer;
    QString vendor;
    QString physicalId;
    QString mac;
    QString groupId;
    QString groupName;
    bool existsInPool = false;

    QnManualResourceSearchEntry() = default;

    QnManualResourceSearchEntry(
        const QString& name,
        const QString& model,
        const QString& url,
        const QString& manufacturer,
        const QString& vendor,
        const QString& physicalId,
        const QString& mac,
        const QString& groupId,
        const QString& groupName,
        bool existsInPool)
        :
        name(name),
        model(model),
        url(url),
        manufacturer(manufacturer),
        vendor(vendor),
        physicalId(physicalId),
        mac(mac),
        groupId(groupId),
        groupName(groupName),
        existsInPool(existsInPool)
    {
    }

    QString toString() const
    {
        return QString(QLatin1String("%1 (%2 - %3)")).arg(name).arg(url).arg(vendor);
    }

    bool isNull() const
    {
        return physicalId.isEmpty();
    }
};

#define QnManualResourceSearchEntry_Fields (name)(url)(manufacturer)(vendor) \
    (existsInPool)(physicalId)(mac)

typedef QList<QnManualResourceSearchEntry> QnManualResourceSearchEntryList;

struct QnManualCameraSearchProcessStatus
{
    nx::vms::api::DeviceSearchStatus status;
    QnManualResourceSearchEntryList cameras;
};

/**%apidoc
 * Current state of the manual Device search: process uuid, status and results found to the moment.
 */
struct NX_VMS_COMMON_API QnManualCameraSearchReply
{
    QnManualCameraSearchReply() {}

    QnManualCameraSearchReply(
        const nx::Uuid &uuid, const QnManualCameraSearchProcessStatus &processStatus)
        :
        processUuid(uuid),
        status(processStatus.status),
        cameras(processStatus.cameras)
    {
    }

    /**%apidoc Id of the manual Device search; to be used by all related API calls. */
    nx::Uuid processUuid;
    nx::vms::api::DeviceSearchStatus status;
    QnManualResourceSearchEntryList cameras;
};

#define QnManualCameraSearchReply_Fields (status)(processUuid)(cameras)

QN_FUSION_DECLARE_FUNCTIONS(QnManualResourceSearchEntry,
    (json)(ubjson)(xml)(csv_record),
    NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnManualCameraSearchReply,
    (json)(ubjson)(xml)(csv_record),
    NX_VMS_COMMON_API)
