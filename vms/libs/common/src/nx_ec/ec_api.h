#ifndef EC_API_H
#define EC_API_H

#include <algorithm>
#include <functional>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QList>

#include <api/model/connection_info.h>

#include <core/resource/videowall_control_message.h>
#include <core/resource_access/user_access_data.h>

#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/email_settings_data.h>
#include <nx/vms/api/data/peer_alive_data.h>
#include <nx/vms/api/data/database_dump_data.h>
#include <nx/vms/api/data/database_dump_to_file_data.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/reverse_connection_data.h>
#include <nx/vms/api/data/client_info_data.h>
#include <nx/vms/api/data/misc_data.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/update_sequence_data.h>
#include <nx/vms/api/data/user_role_data.h>
#include <nx/vms/api/data/system_merge_history_record.h>
#include <nx_ec/managers/abstract_server_manager.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <nx_ec/managers/abstract_user_manager.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <nx_ec/managers/abstract_layout_tour_manager.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <nx_ec/managers/abstract_event_rules_manager.h>
#include <nx_ec/managers/abstract_analytics_manager.h>
#include <nx/vms/api/data/timestamp.h>
#include <nx/vms/time_sync/abstract_time_sync_manager.h>

#include "ec_api_fwd.h"

class QnRestProcessorPool;
class QnHttpConnectionListener;
class QnCommonModule;

namespace nx {
namespace network { class SocketAddress; }

namespace vms { namespace discovery { class Manager; }}
} // namespace nx

//!Contains API classes for the new Server
/*!
    TODO describe all API classes
    \note All methods are thread-safe
*/
namespace ec2 {
class ECConnectionNotificationManager;
class TransactionMessageBusAdapter;
class P2pMessageBus;
class QnAbstractTransactionTransport;

struct QnPeerTimeInfo
{
    QnPeerTimeInfo():
        time(0)
    {
    }

    QnPeerTimeInfo(QnUuid peerId, qint64 time):
        peerId(peerId),
        time(time)
    {
    }

    QnUuid peerId;

    /** Peer system time (UTC, millis from epoch) */
    qint64 time;
};

class AbstractResourceNotificationManager: public QObject
{
Q_OBJECT
public:
signals :
    void statusChanged(
        const QnUuid& resourceId,
        nx::vms::api::ResourceStatus status,
        ec2::NotificationSource source);
    void resourceParamChanged(const nx::vms::api::ResourceParamWithRefData& param);
    void resourceParamRemoved(const nx::vms::api::ResourceParamWithRefData& param);
    void resourceRemoved(const QnUuid& resourceId);
    void resourceStatusRemoved(const QnUuid& resourceId);
};

typedef std::shared_ptr<AbstractResourceNotificationManager>
AbstractResourceNotificationManagerPtr;

/*!
    \note All methods are asynchronous if other not specified
*/
class AbstractResourceManager
{
public:
    virtual ~AbstractResourceManager()
    {
    }

    /*!
        \param handler Functor with params: (ErrorCode, const QnResourceTypeList&)
    */
    template<class TargetType, class HandlerType>
    int getResourceTypes(TargetType* target, HandlerType handler)
    {
        return getResourceTypes(
            std::static_pointer_cast<impl::GetResourceTypesHandler>(
                std::make_shared<impl::CustomGetResourceTypesHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getResourceTypesSync(QnResourceTypeList* const resTypeList)
    {
        return impl::doSyncCall<impl::GetResourceTypesHandler>(
            [&](const impl::GetResourceTypesHandlerPtr& handler)
                {
                    return getResourceTypes(handler);
                },
            resTypeList
        );
    }

    /*!
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int setResourceStatus(
        const QnUuid& resourceId,
        Qn::ResourceStatus status,
        TargetType* target,
        HandlerType handler)
    {
        return setResourceStatus(
            resourceId,
            status,
            std::static_pointer_cast<impl::SetResourceStatusHandler>(
                std::make_shared<impl::CustomSetResourceStatusHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode setResourceStatusSync(const QnUuid& id, Qn::ResourceStatus status)
    {
        QnUuid rezId;
        int (AbstractResourceManager::*fn)(
            const QnUuid&,
            Qn::ResourceStatus,
            impl::SetResourceStatusHandlerPtr) = &AbstractResourceManager::setResourceStatus;
        return impl::doSyncCall<impl::SetResourceStatusHandler>(
            std::bind(fn, this, id, status, std::placeholders::_1),
            &rezId);
    }

    /*!
        \param handler Functor with params:
        (ErrorCode, const nx::vms::api::ResourceParamWithRefDataList&)
    */
    template<class TargetType, class HandlerType>
    int getKvPairs(const QnUuid& resourceId, TargetType* target, HandlerType handler)
    {
        return getKvPairs(
            resourceId,
            std::static_pointer_cast<impl::GetKvPairsHandler>(
                std::make_shared<impl::CustomGetKvPairsHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getKvPairsSync(
        const QnUuid& resourceId,
        nx::vms::api::ResourceParamWithRefDataList* const outData)
    {
        return impl::doSyncCall<impl::GetKvPairsHandler>(
            [=](const impl::GetKvPairsHandlerPtr& handler)
                {
                    return this->getKvPairs(resourceId, handler);
                },
            outData
        );
    }

    /*!
        \param handler Functor with params:
        (ErrorCode, const nx::vms::api::ResourceStatusDataList&)
    */
    template<class TargetType, class HandlerType>
    int getStatusList(const QnUuid& resourceId, TargetType* target, HandlerType handler)
    {
        return getStatusList(
            resourceId,
            std::static_pointer_cast<impl::GetStatusListHandler>(
                std::make_shared<impl::CustomGetStatusListHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getStatusListSync(
        const QnUuid& resourceId,
        nx::vms::api::ResourceStatusDataList* const outData)
    {
        return impl::doSyncCall<impl::GetStatusListHandler>(
            [=](const impl::GetStatusListHandlerPtr& handler)
                {
                    return this->getStatusList(resourceId, handler);
                },
            outData
        );
    }

    /*!
        \param handler Functor with params:
        (ErrorCode, const nx::vms::api::ResourceParamWithRefDataList&)
    */
    template<class TargetType, class HandlerType>
    int save(
        const nx::vms::api::ResourceParamWithRefDataList& kvPairs,
        TargetType* target,
        HandlerType handler)
    {
        return save(
            kvPairs,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode saveSync(const nx::vms::api::ResourceParamWithRefDataList& kvPairs)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [=](const impl::SimpleHandlerPtr& handler)
                {
                    return this->save(kvPairs, handler);
                }
        );
    }

    ErrorCode saveSync(
        const QnUuid& resourceId,
        const nx::vms::api::ResourceParamDataList& properties)
    {
        nx::vms::api::ResourceParamWithRefDataList kvPairs;
        for (const auto& p: properties)
            kvPairs.emplace_back(resourceId, p.name, p.value);
        return saveSync(kvPairs);
    }

    //!Convenient method to remove resource of any type
    /*!
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int remove(const QnUuid& id, TargetType* target, HandlerType handler)
    {
        return remove(
            id,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    template<class TargetType, class HandlerType>
    int remove(const QVector<QnUuid>& idList, TargetType* target, HandlerType handler)
    {
        return remove(
            idList,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

protected:
    virtual int getResourceTypes(impl::GetResourceTypesHandlerPtr handler) = 0;
    virtual int setResourceStatus(
        const QnUuid& resourceId,
        Qn::ResourceStatus status,
        impl::SetResourceStatusHandlerPtr handler) = 0;
    virtual int getKvPairs(const QnUuid& resourceId, impl::GetKvPairsHandlerPtr handler) = 0;
    virtual int getStatusList(
        const QnUuid& resourceId,
        impl::GetStatusListHandlerPtr handler) = 0;
    virtual int save(
        const nx::vms::api::ResourceParamWithRefDataList& kvPairs,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int remove(const QnUuid& resource, impl::SimpleHandlerPtr handler) = 0;
    virtual int remove(
        const QVector<QnUuid>& resourceList,
        impl::SimpleHandlerPtr handler) = 0;
};

class AbstractLicenseNotificationManager: public QObject
{
Q_OBJECT
public:
signals :
    void licenseChanged(QnLicensePtr license);
    void licenseRemoved(QnLicensePtr license);
};

typedef std::shared_ptr<AbstractLicenseNotificationManager> AbstractLicenseNotificationManagerPtr;

/*!
    \note All methods are asynchronous if other not specified
*/
class AbstractLicenseManager
{
public:
    virtual ~AbstractLicenseManager()
    {
    }

    /*!
        \param handler Functor with params: (ErrorCode, const QnLicenseList&)
    */
    template<class TargetType, class HandlerType>
    int getLicenses(TargetType* target, HandlerType handler)
    {
        return getLicenses(
            std::static_pointer_cast<impl::GetLicensesHandler>(
                std::make_shared<impl::CustomGetLicensesHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getLicensesSync(QnLicenseList* const licenseList)
    {
        return impl::doSyncCall<impl::GetLicensesHandler>(
            [=](const impl::GetLicensesHandlerPtr& handler)
                {
                    return this->getLicenses(handler);
                },
            licenseList
        );
    }

    /*!
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int addLicenses(const QList<QnLicensePtr>& licenses, TargetType* target, HandlerType handler)
    {
        return addLicenses(
            licenses,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode addLicensesSync(const QList<QnLicensePtr>& licenses)
    {
        int (AbstractLicenseManager::*fn)(const QList<QnLicensePtr>&, impl::SimpleHandlerPtr) = &
            AbstractLicenseManager::addLicenses;
        return impl::doSyncCall<impl::SimpleHandler>(
            std::bind(fn, this, licenses, std::placeholders::_1));
    }

    template<class TargetType, class HandlerType>
    int removeLicense(const QnLicensePtr& license, TargetType* target, HandlerType handler)
    {
        return removeLicense(
            license,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode removeLicenseSync(const QnLicensePtr& license)
    {
        int (AbstractLicenseManager::*fn)(const QnLicensePtr&, impl::SimpleHandlerPtr) = &
            AbstractLicenseManager::removeLicense;
        return impl::doSyncCall<impl::SimpleHandler>(
            std::bind(fn, this, license, std::placeholders::_1));
    }

protected:
    virtual int getLicenses(impl::GetLicensesHandlerPtr handler) = 0;
    virtual int addLicenses(
        const QList<QnLicensePtr>& licenses,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int removeLicense(const QnLicensePtr& license, impl::SimpleHandlerPtr handler) = 0;
};

class AbstractStoredFileNotificationManager: public QObject
{
Q_OBJECT
public:
signals :
    void added(QString filename);
    void updated(QString filename);
    void removed(QString filename);
};

typedef std::shared_ptr<AbstractStoredFileNotificationManager>
AbstractStoredFileNotificationManagerPtr;

/*!
    \note All methods are asynchronous if other not specified
*/
class AbstractStoredFileManager
{
public:
    virtual ~AbstractStoredFileManager()
    {
    }

    /*!
        \param handler Functor with params: (ErrorCode, const QByteArray&)
    */
    template<class TargetType, class HandlerType>
    int getStoredFile(const QString& filename, TargetType* target, HandlerType handler)
    {
        return getStoredFile(
            filename,
            std::static_pointer_cast<impl::GetStoredFileHandler>(
                std::make_shared<impl::CustomGetStoredFileHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getStoredFileSync(const QString& fileName, QByteArray* fileData)
    {
        int (AbstractStoredFileManager::*fn)(const QString& fname, impl::GetStoredFileHandlerPtr) =
            &AbstractStoredFileManager::getStoredFile;
        return impl::doSyncCall<impl::GetStoredFileHandler>(
            std::bind(fn, this, fileName, std::placeholders::_1),
            fileData);
    }

    /*!
        If file exists, it will be overwritten
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int addStoredFile(
        const QString& filename,
        const QByteArray& data,
        TargetType* target,
        HandlerType handler)
    {
        return addStoredFile(
            filename,
            data,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int deleteStoredFile(const QString& filename, TargetType* target, HandlerType handler)
    {
        return deleteStoredFile(
            filename,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
        \param handler Functor with params: (ErrorCode, const QStringList& filenames)
        \note Only file names are returned
    */
    template<class TargetType, class HandlerType>
    int listDirectory(const QString& folderName, TargetType* target, HandlerType handler)
    {
        return listDirectory(
            folderName,
            std::static_pointer_cast<impl::ListDirectoryHandler>(
                std::make_shared<impl::CustomListDirectoryHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode listDirectorySync(const QString& folderName, QStringList* const outData)
    {
        return impl::doSyncCall<impl::ListDirectoryHandler>(
            [=](const impl::ListDirectoryHandlerPtr& handler)
                {
                    return this->listDirectory(folderName, handler);
                },
            outData
        );
    }

    ErrorCode addStoredFileSync(const QString& folderName, const QByteArray& data)
    {
        int (AbstractStoredFileManager::*fn)(
                const QString&,
                const QByteArray&,
                impl::SimpleHandlerPtr) =
            &AbstractStoredFileManager::addStoredFile;
        return impl::doSyncCall<impl::SimpleHandler>(
            std::bind(fn, this, folderName, data, std::placeholders::_1));
    }

protected:
    virtual int getStoredFile(const QString& filename, impl::GetStoredFileHandlerPtr handler) = 0;
    virtual int addStoredFile(
        const QString& filename,
        const QByteArray& data,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int deleteStoredFile(const QString& filename, impl::SimpleHandlerPtr handler) = 0;
    virtual int listDirectory(
        const QString& folderName,
        impl::ListDirectoryHandlerPtr handler) = 0;
};

class AbstractUpdatesNotificationManager: public QObject
{
Q_OBJECT
public:
signals :
    void updateChunkReceived(const QString& updateId, const QByteArray& data, qint64 offset);
    void updateUploadProgress(const QString& updateId, const QnUuid& peerId, int chunks);
    void updateInstallationRequested(const QString& updateId);
};

typedef std::shared_ptr<AbstractUpdatesNotificationManager> AbstractUpdatesNotificationManagerPtr;

class AbstractUpdatesManager
{
public:
    enum ReplyCode
    {
        NoError = -1,
        UnknownError = -2,
        NoFreeSpace = -3
    };

    virtual ~AbstractUpdatesManager()
    {
    }

    template<class TargetType, class HandlerType>
    int sendUpdatePackageChunk(
        const QString& updateId,
        const QByteArray& data,
        qint64 offset,
        const nx::vms::api::PeerSet& peers,
        TargetType* target,
        HandlerType handler)
    {
        return sendUpdatePackageChunk(
            updateId,
            data,
            offset,
            peers,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    template<class TargetType, class HandlerType>
    int sendUpdateUploadResponce(
        const QString& updateId,
        const QnUuid& peerId,
        int chunks,
        TargetType* target,
        HandlerType handler)
    {
        return sendUpdateUploadResponce(
            updateId,
            peerId,
            chunks,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

protected:
    virtual int sendUpdatePackageChunk(
        const QString& updateId,
        const QByteArray& data,
        qint64 offset,
        const nx::vms::api::PeerSet& peers,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int sendUpdateUploadResponce(
        const QString& updateId,
        const QnUuid& peerId,
        int chunks,
        impl::SimpleHandlerPtr handler) = 0;
};

class AbstractDiscoveryNotificationManager: public QObject
{
    Q_OBJECT
public:
signals:
    void peerDiscoveryRequested(const nx::utils::Url& url);
    void discoveryInformationChanged(const nx::vms::api::DiscoveryData& data, bool addInformation);
    void discoveredServerChanged(const nx::vms::api::DiscoveredServerData& discoveredServer);
    void gotInitialDiscoveredServers(const nx::vms::api::DiscoveredServerDataList& discoveredServers);
};

typedef std::shared_ptr<AbstractDiscoveryNotificationManager>
AbstractDiscoveryNotificationManagerPtr;

class AbstractDiscoveryManager
{
public:
    virtual ~AbstractDiscoveryManager() = default;

    template<class TargetType, class HandlerType>
    int discoverPeer(
        const QnUuid& id,
        const nx::utils::Url& url,
        TargetType* target,
        HandlerType handler)
    {
        return discoverPeer(
            id,
            url,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    template<class TargetType, class HandlerType>
    int addDiscoveryInformation(
        const QnUuid& id,
        const nx::utils::Url& url,
        bool ignore,
        TargetType* target,
        HandlerType handler)
    {
        return addDiscoveryInformation(
            id,
            url,
            ignore,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    template<class TargetType, class HandlerType>
    int removeDiscoveryInformation(
        const QnUuid& id,
        const nx::utils::Url& url,
        bool ignore,
        TargetType* target,
        HandlerType handler)
    {
        return removeDiscoveryInformation(
            id,
            url,
            ignore,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    template<class TargetType, class HandlerType>
    int getDiscoveryData(TargetType* target, HandlerType handler)
    {
        return getDiscoveryData(
            std::static_pointer_cast<impl::GetDiscoveryDataHandler>(
                std::make_shared<impl::CustomGetDiscoveryDataHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getDiscoveryDataSync(nx::vms::api::DiscoveryDataList* const discoveryDataList)
    {
        int (AbstractDiscoveryManager::*fn)(impl::GetDiscoveryDataHandlerPtr) = &
            AbstractDiscoveryManager::getDiscoveryData;
        return impl::doSyncCall<impl::GetDiscoveryDataHandler>(
            std::bind(fn, this, std::placeholders::_1),
            discoveryDataList);
    }

protected:
    virtual int discoverPeer(
        const QnUuid& id,
        const nx::utils::Url& url,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int addDiscoveryInformation(
        const QnUuid& id,
        const nx::utils::Url& url,
        bool ignore,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int removeDiscoveryInformation(
        const QnUuid& id,
        const nx::utils::Url& url,
        bool ignore,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler) = 0;
};

typedef std::shared_ptr<AbstractDiscoveryManager> AbstractDiscoveryManagerPtr;

class AbstractTimeNotificationManager: public QObject
{
Q_OBJECT
public:
    virtual ~AbstractTimeNotificationManager()
    {
    }

signals :
    //!Emitted when synchronized time has been changed
    void timeChanged(qint64 syncTime);
    void primaryTimeServerTimeChanged();
};

typedef std::shared_ptr<AbstractTimeNotificationManager> AbstractTimeNotificationManagerPtr;

class AbstractTimeManager
{
public:
    virtual ~AbstractTimeManager()
    {
    }

    //!Returns current synchronized time (UTC, millis from epoch)
    /*!
        \param handler Functor with params: (ErrorCode, qint64)
    */
    template<class TargetType, class HandlerType>
    int getCurrentTime(TargetType* target, HandlerType handler)
    {
        return getCurrentTimeImpl(
            std::static_pointer_cast<impl::CurrentTimeHandler>(
                std::make_shared<impl::CustomCurrentTimeHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getCurrentTimeSync(qint64* const time)
    {
        int (AbstractTimeManager::*fn)(impl::CurrentTimeHandlerPtr) = &AbstractTimeManager::
            getCurrentTimeImpl;
        return impl::doSyncCall<impl::CurrentTimeHandler>(
            std::bind(fn, this, std::placeholders::_1),
            time);
    }

protected:
    virtual int getCurrentTimeImpl(impl::CurrentTimeHandlerPtr handler) = 0;
    virtual int forcePrimaryTimeServerImpl(
        const QnUuid& serverGuid,
        impl::SimpleHandlerPtr handler) = 0;
};

class AbstractMiscNotificationManager: public QObject
{
    Q_OBJECT
public:
signals:
    void systemIdChangeRequested(
        const QnUuid& systemId, qint64 sysIdTime, nx::vms::api::Timestamp tranLogTime);
    void miscDataChanged(const QString& name, const QString& value);
};

typedef std::shared_ptr<AbstractMiscNotificationManager> AbstractMiscNotificationManagerPtr;

class AbstractMiscManager
{
public:
    virtual ~AbstractMiscManager() = default;

    template<class TargetType, class HandlerType>
    int changeSystemId(
        const QnUuid& systemId,
        qint64 sysIdTime,
        nx::vms::api::Timestamp tranLogTime,
        TargetType* target,
        HandlerType handler)
    {
        return changeSystemId(
            systemId,
            sysIdTime,
            tranLogTime,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode changeSystemIdSync(
        const QnUuid& systemId, qint64 sysIdTime, nx::vms::api::Timestamp tranLogTime)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [=](const impl::SimpleHandlerPtr& handler)
                {
                    return this->changeSystemId(systemId, sysIdTime, tranLogTime, handler);
                }
        );
    }

    ErrorCode cleanupDatabaseSync(bool cleanupDbObjects, bool cleanupTransactionLog)
    {
        int (AbstractMiscManager::*fn)(bool, bool, impl::SimpleHandlerPtr) = &AbstractMiscManager::
            cleanupDatabase;
        return impl::doSyncCall<impl::SimpleHandler>(
            std::bind(fn, this, cleanupDbObjects, cleanupTransactionLog, std::placeholders::_1));
    }

    template<class TargetType, class HandlerType>
    int markLicenseOverflow(bool value, qint64 time, TargetType* target, HandlerType handler)
    {
        return markLicenseOverflow(
            value,
            time,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode markLicenseOverflowSync(bool value, qint64 time)
    {
        int (AbstractMiscManager::*fn)(bool, qint64, impl::SimpleHandlerPtr) =
            &AbstractMiscManager::markLicenseOverflow;

        return impl::doSyncCall<impl::SimpleHandler>(
            std::bind(fn, this, value, time, std::placeholders::_1));
    }

    ErrorCode saveMiscParamSync(const nx::vms::api::MiscData& param)
    {
        int (AbstractMiscManager::*fn)(const nx::vms::api::MiscData&, impl::SimpleHandlerPtr) =
            &AbstractMiscManager::saveMiscParam;

        return impl::doSyncCall<impl::SimpleHandler>(
            std::bind(fn, this, param, std::placeholders::_1));
    }

    template<class TargetType, class HandlerType>
    int saveMiscParam(
        const nx::vms::api::MiscData& data, TargetType* target, HandlerType handler)
    {
        return saveMiscParam(
            data,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    template<class TargetType, class HandlerType>
    int saveRuntimeInfo(
        const nx::vms::api::RuntimeData& data,
        TargetType* target,
        HandlerType handler)
    {
        return saveRuntimeInfo(
            data,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    template<class TargetType, class HandlerType>
    int getMiscParam(const QByteArray& paramName, TargetType* target, HandlerType handler)
    {
        return getMiscParam(
            paramName,
            std::static_pointer_cast<impl::GetMiscParamHandler>(
                std::make_shared<impl::CustomGetMiscParamHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    ErrorCode getMiscParamSync(
        const QByteArray& paramName, nx::vms::api::MiscData* const outData)
    {
        return impl::doSyncCall<impl::GetMiscParamHandler>(
            [=](const impl::GetMiscParamHandlerPtr& handler)
                {
                    return this->getMiscParam(paramName, handler);
                },
            outData);
    }

    ErrorCode saveSystemMergeHistoryRecord(const nx::vms::api::SystemMergeHistoryRecord& param)
    {
        int (AbstractMiscManager::*fn)(
                const nx::vms::api::SystemMergeHistoryRecord&,
                impl::SimpleHandlerPtr) =
            &AbstractMiscManager::saveSystemMergeHistoryRecord;
        return impl::doSyncCall<impl::SimpleHandler>(
            std::bind(fn, this, param, std::placeholders::_1));
    }

    template<class TargetType, class HandlerType>
    int getSystemMergeHistory(TargetType* target, HandlerType handler)
    {
        return getSystemMergeHistory(
            std::static_pointer_cast<impl::GetSystemMergeHistoryHandlerPtr>(
                std::make_shared<impl::CustomGetSystemMergeHistoryHandler<TargetType, HandlerType>
                >(target, handler)));
    }

    ErrorCode getSystemMergeHistorySync(nx::vms::api::SystemMergeHistoryRecordList* const outData)
    {
        return impl::doSyncCall<impl::GetSystemMergeHistoryHandler>(
            [this](const impl::GetSystemMergeHistoryHandlerPtr& handler)
                {
                    return this->getSystemMergeHistory(handler);
                },
            outData);
    }

protected:
    virtual int changeSystemId(
        const QnUuid& systemId,
        qint64 sysIdTime,
        nx::vms::api::Timestamp tranLogTime,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int markLicenseOverflow(bool value, qint64 time, impl::SimpleHandlerPtr handler) = 0;
    virtual int cleanupDatabase(
        bool cleanupDbObjects,
        bool cleanupTransactionLog,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int saveMiscParam(
        const nx::vms::api::MiscData& param, impl::SimpleHandlerPtr handler) = 0;
    virtual int saveRuntimeInfo(
        const nx::vms::api::RuntimeData& data,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int getMiscParam(
        const QByteArray& paramName,
        impl::GetMiscParamHandlerPtr handler) = 0;

    virtual int saveSystemMergeHistoryRecord(
        const nx::vms::api::SystemMergeHistoryRecord& param,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int getSystemMergeHistory(impl::GetSystemMergeHistoryHandlerPtr handler) = 0;
};

typedef std::shared_ptr<AbstractMiscManager> AbstractMiscManagerPtr;

/*!
    \note All methods are asynchronous if other not specified
*/
class AbstractECConnection: public QObject
{
Q_OBJECT

public:
    virtual ~AbstractECConnection()
    {
    }

    virtual QnConnectionInfo connectionInfo() const = 0;

    /**
     * Changes url client currently uses to connect to server. Required to handle situations
     * like user password change, server port change or systems merge.
     */
    virtual void updateConnectionUrl(const nx::utils::Url& url) = 0;

    /** !Calling this method starts notifications delivery by emitting corresponding signals of
        the corresponding manager
        \note Calling entity MUST connect to all interesting signals prior to calling this method
        so that received data is consistent
    */
    virtual void startReceivingNotifications() = 0;
    virtual void stopReceivingNotifications() = 0;

    virtual void addRemotePeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& _url) = 0;
    virtual void deleteRemotePeer(const QnUuid& id) = 0;

    virtual nx::vms::api::Timestamp getTransactionLogTime() const = 0;
    virtual void setTransactionLogTime(nx::vms::api::Timestamp value) = 0;

    virtual AbstractResourceManagerPtr getResourceManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractMediaServerManagerPtr getMediaServerManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractCameraManagerPtr getCameraManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractLicenseManagerPtr getLicenseManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractEventRulesManagerPtr getEventRulesManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractUserManagerPtr getUserManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractLayoutManagerPtr getLayoutManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractLayoutTourManagerPtr getLayoutTourManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractVideowallManagerPtr getVideowallManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractStoredFileManagerPtr getStoredFileManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractUpdatesManagerPtr getUpdatesManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractMiscManagerPtr getMiscManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractDiscoveryManagerPtr getDiscoveryManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractWebPageManagerPtr getWebPageManager(
        const Qn::UserAccessData& userAccessData) = 0;
    virtual AbstractAnalyticsManagerPtr getAnalyticsManager(
        const Qn::UserAccessData& userAccessData) = 0;

    virtual AbstractResourceNotificationManagerPtr resourceNotificationManager() = 0;
    virtual AbstractMediaServerNotificationManagerPtr mediaServerNotificationManager() = 0;
    virtual AbstractCameraNotificationManagerPtr cameraNotificationManager() = 0;
    virtual AbstractLayoutNotificationManagerPtr layoutNotificationManager() = 0;
    virtual AbstractLayoutTourNotificationManagerPtr layoutTourNotificationManager() = 0;
    virtual AbstractVideowallNotificationManagerPtr videowallNotificationManager() = 0;
    virtual AbstractStoredFileNotificationManagerPtr storedFileNotificationManager() = 0;
    virtual AbstractUpdatesNotificationManagerPtr updatesNotificationManager() = 0;
    virtual AbstractMiscNotificationManagerPtr miscNotificationManager() = 0;
    virtual AbstractDiscoveryNotificationManagerPtr discoveryNotificationManager() = 0;
    virtual AbstractWebPageNotificationManagerPtr webPageNotificationManager() = 0;
    virtual AbstractAnalyticsNotificationManagerPtr analyticsNotificationManager() = 0;
    virtual AbstractLicenseNotificationManagerPtr licenseNotificationManager() = 0;
    virtual AbstractTimeNotificationManagerPtr timeNotificationManager() = 0;
    virtual AbstractBusinessEventNotificationManagerPtr businessEventNotificationManager() = 0;
    virtual AbstractUserNotificationManagerPtr userNotificationManager() = 0;

    virtual QnCommonModule* commonModule() const = 0;

    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer,
        int* distance,
        nx::network::SocketAddress* knownPeerAddress) const = 0;
    virtual TransactionMessageBusAdapter* messageBus() const = 0;
    virtual nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager() const = 0;

    virtual ECConnectionNotificationManager* notificationManager()
    {
        NX_ASSERT(0);
        return nullptr;
    }

    /*!
        \param handler Functor with params: (requestID, ErrorCode, QByteArray dbFile)
    */
    template<class TargetType, class HandlerType>
    int dumpDatabaseAsync(TargetType* target, HandlerType handler)
    {
        return dumpDatabaseAsync(
            std::static_pointer_cast<impl::DumpDatabaseHandler>(
                std::make_shared<impl::CustomDumpDatabaseHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
        \param handler Functor with params: (requestID, ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int dumpDatabaseToFileAsync(
        const QString& dumpFilePath,
        TargetType* target,
        HandlerType handler)
    {
        return dumpDatabaseToFileAsync(
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    dumpFilePath,
                    target,
                    handler)));
    }

    ErrorCode dumpDatabaseToFileSync(const QString& dumpFilePath)
    {
        int (AbstractECConnection::*fn)(const QString&, impl::SimpleHandlerPtr) = &
            AbstractECConnection::dumpDatabaseToFileAsync;
        return impl::doSyncCall<impl::SimpleHandler>(
            std::bind(fn, this, dumpFilePath, std::placeholders::_1));
    }

    /*!
        \param handler Functor with params: (requestID, ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int restoreDatabaseAsync(
        const nx::vms::api::DatabaseDumpData& data,
        TargetType* target,
        HandlerType handler)
    {
        return restoreDatabaseAsync(
            data,
            std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    //!Cancel running async request
    /*!
        \warning Request handler may still be called after return of this method, since request
        could already have been completed and resulte posted to handler
    */
    //virtual void cancelRequest( int requestID ) = 0;
signals :
    //!Delivers all resources found in Server
    /*!
        This signal is emitted after starting notifications delivery by call to
        \a AbstractECConnection::startReceivingNotifications
            if full synchronization is requested
        \param resTypes
        \param resList All resources (servers, cameras, users, layouts)
        \param kvPairs Parameters of resources
        \param licenses
        \param cameraHistoryItems
    */
    void initNotification(const nx::vms::api::FullInfoData& fullData);
    void runtimeInfoChanged(const nx::vms::api::RuntimeData& runtimeInfo);
    void runtimeInfoRemoved(const nx::vms::api::IdData& runtimeInfo);

    void reverseConnectionRequested(const nx::vms::api::ReverseConnectionData& reverseConnetionData);

    void remotePeerFound(QnUuid data, nx::vms::api::PeerType peerType);
    void remotePeerLost(QnUuid data, nx::vms::api::PeerType peerType);
    void remotePeerUnauthorized(const QnUuid& id);
    void newDirectConnectionEstablished(QnAbstractTransactionTransport* transport);

    void settingsChanged(nx::vms::api::ResourceParamDataList settings);

    void databaseDumped();

protected:
    virtual int dumpDatabaseAsync(impl::DumpDatabaseHandlerPtr handler) = 0;
    virtual int dumpDatabaseToFileAsync(
        const QString& dumpFilePath,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int restoreDatabaseAsync(
        const nx::vms::api::DatabaseDumpData& data,
        impl::SimpleHandlerPtr handler) = 0;
};

/*!
    \note All methods are asynchronous if other not specified
*/
class AbstractECConnectionFactory: public QObject, public /*mixin*/ QnCommonModuleAware
{
Q_OBJECT

public:
    AbstractECConnectionFactory(QnCommonModule* commonModule);

    virtual ~AbstractECConnectionFactory()
    {
    }

    /*!
        \param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int testConnection(const nx::utils::Url& addr, TargetType* target, HandlerType handler)
    {
        return testConnectionAsync(
            addr,
            std::static_pointer_cast<impl::TestConnectionHandler>(
                std::make_shared<impl::CustomTestConnectionHandler<TargetType, HandlerType>>(
                    target,
                    handler)));
    }

    /*!
        \param addr Empty url designates local Server ("local" means dll linked to executable,
        not Server running on local host)
        \param handler Functor with params: (ErrorCode, AbstractECConnectionPtr)
    */
    template<class TargetType, class HandlerType>
    int connect(
        const nx::utils::Url& addr,
        const nx::vms::api::ClientInfoData& clientInfo,
        TargetType* target,
        HandlerType handler)
    {
        auto cch = std::make_shared<impl::CustomConnectHandler<TargetType, HandlerType>>(
            target,
            handler);
        return connectAsync(addr, clientInfo, std::static_pointer_cast<impl::ConnectHandler>(cch));
    }

    ErrorCode connectSync(
        const nx::utils::Url& addr,
        const nx::vms::api::ClientInfoData& clientInfo,
        AbstractECConnectionPtr* const connection)
    {
        auto call = std::bind(
            std::mem_fn(&AbstractECConnectionFactory::connectAsync),
            this,
            addr,
            clientInfo,
            std::placeholders::_1);
        return impl::doSyncCall<impl::ConnectHandler>(call, connection);
    }

    virtual TransactionMessageBusAdapter* messageBus() const = 0;
    virtual nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager() const = 0;

    virtual void shutdown()
    {
    }

protected:
    virtual int testConnectionAsync(
        const nx::utils::Url& addr,
        impl::TestConnectionHandlerPtr handler) = 0;
    virtual int connectAsync(
        const nx::utils::Url& addr,
        const nx::vms::api::ClientInfoData& clientInfo,
        impl::ConnectHandlerPtr handler) = 0;
};
}

Q_DECLARE_METATYPE(ec2::QnPeerTimeInfo) ;

Q_DECLARE_METATYPE(ec2::QnPeerTimeInfoList) ;

#endif  //EC_API_H
