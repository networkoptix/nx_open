// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/api/data/license_data.h>
#include <nx/vms/common/system_context_aware.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common::saas {

/*
 *  Cloud/saas licenses is backward compatible with licenses v1.
 *  So, to control localRecording old class 'UsageHelper' can be used in the same way
 *  for local and cloud licenses.
 *  This class is intended to calculate other (new) services for cloud licenses
 */
class NX_VMS_COMMON_API CloudServiceUsageHelper: public QObject
{
public:
    CloudServiceUsageHelper(QObject* parent = nullptr);
};

 /*
  *  Helper class to calculate integration licenses usages.
  */
class NX_VMS_COMMON_API IntegrationServiceUsageHelper:
    public CloudServiceUsageHelper,
    public /*mixin*/ SystemContextAware
{
public:
    IntegrationServiceUsageHelper(SystemContext* context, QObject* parent = nullptr);

    /*
     *  @param id Integration id.
     *  @return Information about available licenses per integration.
     */
    nx::vms::api::LicenseSummaryData info(const QnUuid& id);

    QMap<QnUuid, nx::vms::api::LicenseSummaryData> allInfo() const;

    /*
     *  @return true if there are not enough licenses for any integration.
     */
    bool isOverflow() const;


    struct Propose
    {
        QnUuid resourceId;
        QSet<QnUuid> integrations;
    };

    /* Propose change in integration usage for some resource.
     *  @param resourceId Resource Id.
     *  @param integrations Set of used integration.
     */
    void proposeChange(const QnUuid& resourceId, const QSet<QnUuid>& integrations);

    void proposeChange(const std::vector<Propose>& data);

    void invalidateCache();
private:
    void updateCache();
    void updateCacheUnsafe() const;
private:
    //Summary by integrationId.
    mutable std::optional<QMap<QnUuid, nx::vms::api::LicenseSummaryData>> m_cache;
    mutable nx::Mutex m_mutex;
};

/*
  *  Helper class to calculate integration licenses usages.
  */
class NX_VMS_COMMON_API CloudStorageServiceUsageHelper:
    public CloudServiceUsageHelper,
    public /*mixin*/ SystemContextAware
{
public:
    CloudStorageServiceUsageHelper(SystemContext* context, QObject* parent = nullptr);

    /*
     *  @return true if there are not enough licenses for any integration.
     */
    bool isOverflow() const;

    /*
     *  @return Information about available license usages per megapixels.
     */
    std::map<int, nx::vms::api::LicenseSummaryData> allInfo() const;

    /* Propose change that resources are used for cloud storage.
     *  @param devices New full set of resources that is going to be used.
     */
    void setUsedDevices(const QSet<QnUuid>& devices);

    void invalidateCache();

private:
    void updateCache();
    void updateCacheUnsafe() const;
    void borrowUsages() const;
    void calculateAvailableUnsafe() const;
private:
    //Summary by megapixels.
    mutable std::optional<std::map<int, nx::vms::api::LicenseSummaryData>> m_cache;
    mutable nx::Mutex m_mutex;
};

} // nx::vms::common::saas
