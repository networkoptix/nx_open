#ifndef EC_API_H
#define EC_API_H

#include <algorithm>
#include <functional>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QList>

#include "network/module_information.h"

#include <api/model/connection_info.h>
#include <api/model/email_attachment.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/videowall_control_message.h>
#include <core/resource_access/user_access_data.h>

#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>
#include <nx_ec/data/api_resource_data.h>
#include <nx_ec/data/api_email_data.h>
#include <nx_ec/data/api_peer_alive_data.h>
#include <nx_ec/data/api_time_data.h>
#include <nx_ec/data/api_license_overflow_data.h>
#include <nx_ec/data/api_discovery_data.h>
#include <nx_ec/data/api_camera_history_data.h>
#include <nx_ec/data/api_reverse_connection_data.h>
#include <nx_ec/data/api_client_info_data.h>
#include <nx_ec/data/api_camera_attributes_data.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/data/api_access_rights_data.h>
#include <nx_ec/data/api_user_role_data.h>
#include <nx_ec/data/api_misc_data.h>
#include "nx_ec/managers/abstract_server_manager.h"
#include "nx_ec/managers/abstract_camera_manager.h"
#include "nx_ec/managers/abstract_user_manager.h"
#include "nx_ec/managers/abstract_layout_manager.h"
#include <nx_ec/managers/abstract_layout_tour_manager.h>
#include "nx_ec/managers/abstract_webpage_manager.h"
#include "nx_ec/managers/abstract_videowall_manager.h"

#include "ec_api_fwd.h"
#include "transaction_timestamp.h"

class QnRestProcessorPool;
class QnHttpConnectionListener;
class QnCommonModule;
struct QnModuleInformation;
namespace nx { namespace vms { namespace discovery { class Manager; } } }

//!Contains API classes for the new Server
/*!
    TODO describe all API classes
    \note All methods are thread-safe
*/
namespace ec2
{
    class ECConnectionNotificationManager;
    class TransactionMessageBusAdapter;
    class P2pMessageBus;
    class QnDistributedMutexManager;
    class TimeSynchronizationManager;

    struct QnPeerTimeInfo {

        QnPeerTimeInfo():
            time(0){}
        QnPeerTimeInfo(QnUuid peerId, qint64 time):
            peerId(peerId), time(time){}

        QnUuid peerId;

        /** Peer system time (UTC, millis from epoch) */
        qint64 time;
    };

    class AbstractResourceNotificationManager : public QObject
    {
        Q_OBJECT
    public:
    signals:
        void statusChanged( const QnUuid& resourceId, Qn::ResourceStatus status, ec2::NotificationSource source);
        void resourceParamChanged( const ApiResourceParamWithRefData& param );
        void resourceParamRemoved( const ApiResourceParamWithRefData& param );
        void resourceRemoved(const QnUuid& resourceId);
        void resourceStatusRemoved(const QnUuid& resourceId);
    };

    typedef std::shared_ptr<AbstractResourceNotificationManager> AbstractResourceNotificationManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractResourceManager
    {
    public:
        virtual ~AbstractResourceManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnResourceTypeList&)
        */
        template<class TargetType, class HandlerType>
        int getResourceTypes( TargetType* target, HandlerType handler ) {
            return getResourceTypes( std::static_pointer_cast<impl::GetResourceTypesHandler>(std::make_shared<impl::CustomGetResourceTypesHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getResourceTypesSync( QnResourceTypeList* const resTypeList ) {
            return impl::doSyncCall<impl::GetResourceTypesHandler>(
                [&](const impl::GetResourceTypesHandlerPtr &handler) {
                    return getResourceTypes(handler);
                },
                resTypeList
            );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int setResourceStatus( const QnUuid& resourceId, Qn::ResourceStatus status, TargetType* target, HandlerType handler ) {
            return setResourceStatus(resourceId, status, std::static_pointer_cast<impl::SetResourceStatusHandler>(std::make_shared<impl::CustomSetResourceStatusHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode setResourceStatusSync( const QnUuid& id, Qn::ResourceStatus status) {
            QnUuid rezId;
            int(AbstractResourceManager::*fn)(const QnUuid&, Qn::ResourceStatus, impl::SetResourceStatusHandlerPtr) = &AbstractResourceManager::setResourceStatus;
            return impl::doSyncCall<impl::SetResourceStatusHandler>( std::bind(fn, this, id, status, std::placeholders::_1), &rezId );
        }

        /*!
            \param handler Functor with params: (ErrorCode, const ApiResourceParamWithRefDataList&)
        */
        template<class TargetType, class HandlerType>
        int getKvPairs( const QnUuid& resourceId, TargetType* target, HandlerType handler )
        {
            return getKvPairs( resourceId, std::static_pointer_cast<impl::GetKvPairsHandler>(std::make_shared<impl::CustomGetKvPairsHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getKvPairsSync(const QnUuid& resourceId, ApiResourceParamWithRefDataList* const outData)
        {
            return impl::doSyncCall<impl::GetKvPairsHandler>(
                [=](const impl::GetKvPairsHandlerPtr &handler) {
                    return this->getKvPairs(resourceId, handler);
                },
                outData
            );
        }

        /*!
            \param handler Functor with params: (ErrorCode, const ApiResourceStatusDataList&)
        */
        template<class TargetType, class HandlerType>
        int getStatusList( const QnUuid& resourceId, TargetType* target, HandlerType handler ) {
            return getStatusList( resourceId, std::static_pointer_cast<impl::GetStatusListHandler>(std::make_shared<impl::CustomGetStatusListHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getStatusListSync(const QnUuid& resourceId, ApiResourceStatusDataList* const outData)
        {
            return impl::doSyncCall<impl::GetStatusListHandler>(
                [=](const impl::GetStatusListHandlerPtr &handler) {
                    return this->getStatusList(resourceId, handler);
                },
                outData
            );
        }

        /*!
            \param handler Functor with params: (ErrorCode, const ApiResourceParamWithRefDataList&)
        */
        template<class TargetType, class HandlerType>
        int save(const ec2::ApiResourceParamWithRefDataList& kvPairs, TargetType* target, HandlerType handler )
        {
            return save(kvPairs, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode saveSync(const ec2::ApiResourceParamWithRefDataList& kvPairs)
        {
            return impl::doSyncCall<impl::SimpleHandler>(
                [=](const impl::SimpleHandlerPtr &handler)
                {
                    return this->save(kvPairs, handler);
                }
            );
        }

        ErrorCode saveSync(const QnUuid& resourceId, const ec2::ApiResourceParamDataList& properties)
        {
            ApiResourceParamWithRefDataList kvPairs;
            for (const auto& p: properties)
                kvPairs.push_back(ApiResourceParamWithRefData(resourceId, p.name, p.value));
            return saveSync(kvPairs);
        }

        //!Convenient method to remove resource of any type
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int remove( const QnUuid& id, TargetType* target, HandlerType handler ) {
            return remove( id, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        template<class TargetType, class HandlerType>
        int remove( const QVector<QnUuid>& idList, TargetType* target, HandlerType handler ) {
            return remove( idList, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    protected:
        virtual int getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) = 0;
        virtual int setResourceStatus( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler ) = 0;
        virtual int getKvPairs( const QnUuid &resourceId, impl::GetKvPairsHandlerPtr handler ) = 0;
        virtual int getStatusList( const QnUuid &resourceId, impl::GetStatusListHandlerPtr handler ) = 0;
        virtual int save(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SimpleHandlerPtr handler ) = 0;
        virtual int remove( const QnUuid& resource, impl::SimpleHandlerPtr handler ) = 0;
        virtual int remove( const QVector<QnUuid>& resourceList, impl::SimpleHandlerPtr handler ) = 0;
    };

    class AbstractLicenseNotificationManager : public QObject
    {
        Q_OBJECT
    public:
    signals:
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
        virtual ~AbstractLicenseManager() {}
        /*!
            \param handler Functor with params: (ErrorCode, const QnLicenseList&)
        */
        template<class TargetType, class HandlerType> int getLicenses( TargetType* target, HandlerType handler ) {
            return getLicenses( std::static_pointer_cast<impl::GetLicensesHandler>(std::make_shared<impl::CustomGetLicensesHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode getLicensesSync(QnLicenseList* const licenseList ) {
            return impl::doSyncCall<impl::GetLicensesHandler>(
                [=](const impl::GetLicensesHandlerPtr &handler) {
                    return this->getLicenses(handler);
                },
                licenseList
            );
        }


        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int addLicenses( const QList<QnLicensePtr>& licenses, TargetType* target, HandlerType handler ) {
            return addLicenses( licenses, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode addLicensesSync(const QList<QnLicensePtr>& licenses) {
            int(AbstractLicenseManager::*fn)(const QList<QnLicensePtr>&, impl::SimpleHandlerPtr) = &AbstractLicenseManager::addLicenses;
            return impl::doSyncCall<impl::SimpleHandler>( std::bind(fn, this, licenses, std::placeholders::_1));
        }


        template<class TargetType, class HandlerType> int removeLicense( const QnLicensePtr& license, TargetType* target, HandlerType handler ) {
            return removeLicense( license, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode removeLicenseSync(const QnLicensePtr& license)
        {
            int(AbstractLicenseManager::*fn)(const QnLicensePtr&, impl::SimpleHandlerPtr) = &AbstractLicenseManager::removeLicense;
            return impl::doSyncCall<impl::SimpleHandler>(std::bind(fn, this, license, std::placeholders::_1));
        }

    protected:
        virtual int getLicenses( impl::GetLicensesHandlerPtr handler ) = 0;
        virtual int addLicenses( const QList<QnLicensePtr>& licenses, impl::SimpleHandlerPtr handler ) = 0;
        virtual int removeLicense( const QnLicensePtr& license, impl::SimpleHandlerPtr handler ) = 0;
    };


    class AbstractBusinessEventNotificationManager : public QObject
    {
        Q_OBJECT
    public:
    signals:
        void addedOrUpdated( nx::vms::event::RulePtr businessRule, NotificationSource source);
        void removed( QnUuid id );
        void businessActionBroadcasted( const nx::vms::event::AbstractActionPtr& businessAction );
        void businessRuleReset( const ec2::ApiBusinessRuleDataList& rules );
        void gotBroadcastAction(const nx::vms::event::AbstractActionPtr& action);
        void execBusinessAction(const nx::vms::event::AbstractActionPtr& action);
    };

    typedef std::shared_ptr<AbstractBusinessEventNotificationManager> AbstractBusinessEventNotificationManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractBusinessEventManager
    {
    public:
        virtual ~AbstractBusinessEventManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const nx::vms::event::RuleList&)
        */
        template<class TargetType, class HandlerType> int getBusinessRules( TargetType* target, HandlerType handler ) {
            return getBusinessRules( std::static_pointer_cast<impl::GetBusinessRulesHandler>(
                std::make_shared<impl::CustomGetBusinessRulesHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getBusinessRulesSync(nx::vms::event::RuleList* const businessEventList ) {
            int(AbstractBusinessEventManager::*fn)(impl::GetBusinessRulesHandlerPtr) = &AbstractBusinessEventManager::getBusinessRules;
            return impl::doSyncCall<impl::GetBusinessRulesHandler>( std::bind(fn, this, std::placeholders::_1), businessEventList );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save( const nx::vms::event::RulePtr& rule, TargetType* target, HandlerType handler ) {
            return save( rule, std::static_pointer_cast<impl::SaveBusinessRuleHandler>(
                std::make_shared<impl::CustomSaveBusinessRuleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int deleteRule( QnUuid ruleId, TargetType* target, HandlerType handler ) {
            return deleteRule( ruleId, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int broadcastBusinessAction( const nx::vms::event::AbstractActionPtr& businessAction, TargetType* target, HandlerType handler ) {
            return broadcastBusinessAction( businessAction, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        template<class TargetType, class HandlerType> int sendBusinessAction( const nx::vms::event::AbstractActionPtr& businessAction, const QnUuid& dstPeer, TargetType* target, HandlerType handler ) {
            return sendBusinessAction( businessAction, dstPeer, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }


        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int resetBusinessRules( TargetType* target, HandlerType handler ) {
            return resetBusinessRules( std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    private:
        virtual int getBusinessRules( impl::GetBusinessRulesHandlerPtr handler ) = 0;
        virtual int save( const nx::vms::event::RulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler ) = 0;
        virtual int deleteRule( QnUuid ruleId, impl::SimpleHandlerPtr handler ) = 0;
        virtual int broadcastBusinessAction( const nx::vms::event::AbstractActionPtr& businessAction, impl::SimpleHandlerPtr handler ) = 0;
        virtual int sendBusinessAction( const nx::vms::event::AbstractActionPtr& businessAction, const QnUuid& id, impl::SimpleHandlerPtr handler ) = 0;
        virtual int resetBusinessRules( impl::SimpleHandlerPtr handler ) = 0;
    };

    class AbstractStoredFileNotificationManager: public QObject
    {
        Q_OBJECT
    public:
    signals:
        void added( QString filename );
        void updated( QString filename );
        void removed( QString filename );
    };

    typedef std::shared_ptr<AbstractStoredFileNotificationManager> AbstractStoredFileNotificationManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractStoredFileManager
    {
    public:
        virtual ~AbstractStoredFileManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QByteArray&)
        */
        template<class TargetType, class HandlerType> int getStoredFile( const QString& filename, TargetType* target, HandlerType handler ) {
            return getStoredFile( filename, std::static_pointer_cast<impl::GetStoredFileHandler>(
                std::make_shared<impl::CustomGetStoredFileHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getStoredFileSync(const QString& fileName, QByteArray* fileData) {
            int(AbstractStoredFileManager::*fn)(const QString& fname, impl::GetStoredFileHandlerPtr) = &AbstractStoredFileManager::getStoredFile;
            return impl::doSyncCall<impl::GetStoredFileHandler>(std::bind(fn, this, fileName, std::placeholders::_1), fileData);
        }

        /*!
            If file exists, it will be overwritten
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int addStoredFile( const QString& filename, const QByteArray& data, TargetType* target, HandlerType handler ) {
            return addStoredFile( filename, data, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int deleteStoredFile( const QString& filename, TargetType* target, HandlerType handler ) {
            return deleteStoredFile( filename, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QStringList& filenames)
            \note Only file names are returned
        */
        template<class TargetType, class HandlerType> int listDirectory( const QString& folderName, TargetType* target, HandlerType handler ) {
            return listDirectory( folderName, std::static_pointer_cast<impl::ListDirectoryHandler>(
                std::make_shared<impl::CustomListDirectoryHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode listDirectorySync(const QString& folderName, QStringList* const outData)
        {
            return impl::doSyncCall<impl::ListDirectoryHandler>(
                [=](const impl::ListDirectoryHandlerPtr &handler)
                {
                    return this->listDirectory(folderName, handler);
                },
                outData
            );
        }

        ErrorCode addStoredFileSync(const QString& folderName, const QByteArray& data)
        {
            int(AbstractStoredFileManager::*fn)(
                const QString&,
                const QByteArray&,
                impl::SimpleHandlerPtr) =
                &AbstractStoredFileManager::addStoredFile;
            return impl::doSyncCall<impl::SimpleHandler>(
                std::bind(fn, this, folderName, data, std::placeholders::_1));
        }

    protected:
        virtual int getStoredFile( const QString& filename, impl::GetStoredFileHandlerPtr handler ) = 0;
        virtual int addStoredFile( const QString& filename, const QByteArray& data, impl::SimpleHandlerPtr handler ) = 0;
        virtual int deleteStoredFile( const QString& filename, impl::SimpleHandlerPtr handler ) = 0;
        virtual int listDirectory( const QString& folderName, impl::ListDirectoryHandlerPtr handler ) = 0;
    };


    class AbstractUpdatesNotificationManager: public QObject
    {
        Q_OBJECT
    public:
    signals:
        void updateChunkReceived(const QString &updateId, const QByteArray &data, qint64 offset);
        void updateUploadProgress(const QString &updateId, const QnUuid &peerId, int chunks);
        void updateInstallationRequested(const QString &updateId);
    };

    typedef std::shared_ptr<AbstractUpdatesNotificationManager> AbstractUpdatesNotificationManagerPtr;

    class AbstractUpdatesManager {
    public:
        enum ReplyCode {
            NoError = -1,
            UnknownError = -2,
            NoFreeSpace = -3
        };

        virtual ~AbstractUpdatesManager() {}

        template<class TargetType, class HandlerType> int sendUpdatePackageChunk(const QString &updateId, const QByteArray &data, qint64 offset, const QnPeerSet &peers, TargetType *target, HandlerType handler) {
            return sendUpdatePackageChunk(updateId, data, offset, peers, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType> int sendUpdateUploadResponce(const QString &updateId, const QnUuid &peerId, int chunks, TargetType *target, HandlerType handler) {
            return sendUpdateUploadResponce(updateId, peerId, chunks, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

    protected:
        virtual int sendUpdatePackageChunk(const QString &updateId, const QByteArray &data, qint64 offset, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) = 0;
        virtual int sendUpdateUploadResponce(const QString &updateId, const QnUuid &peerId, int chunks, impl::SimpleHandlerPtr handler) = 0;
    };


    class AbstractDiscoveryNotificationManager : public QObject
    {
        Q_OBJECT
    public:
    signals:
        void peerDiscoveryRequested(const nx::utils::Url &url);
        void discoveryInformationChanged(const ApiDiscoveryData &data, bool addInformation);
        void discoveredServerChanged(const ApiDiscoveredServerData &discoveredServer);
        void gotInitialDiscoveredServers(const ApiDiscoveredServerDataList &discoveredServers);
    };

    typedef std::shared_ptr<AbstractDiscoveryNotificationManager> AbstractDiscoveryNotificationManagerPtr;

    class AbstractDiscoveryManager {
    public:
        template<class TargetType, class HandlerType> int discoverPeer(const QnUuid &id, const nx::utils::Url &url, TargetType *target, HandlerType handler) {
            return discoverPeer(id, url, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType> int addDiscoveryInformation(const QnUuid &id, const nx::utils::Url&url, bool ignore, TargetType *target, HandlerType handler) {
            return addDiscoveryInformation(id, url, ignore, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType> int removeDiscoveryInformation(const QnUuid &id, const nx::utils::Url &url, bool ignore, TargetType *target, HandlerType handler) {
            return removeDiscoveryInformation(id, url, ignore, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType> int getDiscoveryData(TargetType *target, HandlerType handler) {
            return getDiscoveryData(std::static_pointer_cast<impl::GetDiscoveryDataHandler>(
                std::make_shared<impl::CustomGetDiscoveryDataHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getDiscoveryDataSync(ApiDiscoveryDataList* const discoveryDataList) {
            int(AbstractDiscoveryManager::*fn)(impl::GetDiscoveryDataHandlerPtr) = &AbstractDiscoveryManager::getDiscoveryData;
            return impl::doSyncCall<impl::GetDiscoveryDataHandler>(std::bind(fn, this, std::placeholders::_1), discoveryDataList);
        }

    protected:
        virtual int discoverPeer(const QnUuid &id, const nx::utils::Url &url, impl::SimpleHandlerPtr handler) = 0;
        virtual int addDiscoveryInformation(const QnUuid &id, const nx::utils::Url &url, bool ignore, impl::SimpleHandlerPtr handler) = 0;
        virtual int removeDiscoveryInformation(const QnUuid &id, const nx::utils::Url &url, bool ignore, impl::SimpleHandlerPtr handler) = 0;
        virtual int getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler) = 0;
    };
    typedef std::shared_ptr<AbstractDiscoveryManager> AbstractDiscoveryManagerPtr;


    class AbstractTimeNotificationManager : public QObject
    {
        Q_OBJECT
    public:
        virtual ~AbstractTimeNotificationManager() {}

    signals:
        //!Emitted when synchronized time has been changed
        void timeChanged( qint64 syncTime );
    };

    typedef std::shared_ptr<AbstractTimeNotificationManager> AbstractTimeNotificationManagerPtr;

    class AbstractTimeManager
    {
    public:
        virtual ~AbstractTimeManager() {}

        //!Returns current synchronized time (UTC, millis from epoch)
        /*!
            \param handler Functor with params: (ErrorCode, qint64)
        */
        template<class TargetType, class HandlerType> int getCurrentTime( TargetType* target, HandlerType handler ) {
            return getCurrentTimeImpl( std::static_pointer_cast<impl::CurrentTimeHandler>(
                std::make_shared<impl::CustomCurrentTimeHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getCurrentTimeSync(qint64* const time) {
            int(AbstractTimeManager::*fn)(impl::CurrentTimeHandlerPtr) = &AbstractTimeManager::getCurrentTimeImpl;
            return impl::doSyncCall<impl::CurrentTimeHandler>( std::bind(fn, this, std::placeholders::_1), time );
        }

        //!Set peer identified by \a serverGuid to be primary time server (every other peer synchronizes time with server \a serverGuid)
        template<class TargetType, class HandlerType> int forcePrimaryTimeServer( const QnUuid& serverGuid, TargetType* target, HandlerType handler )
        {
            return forcePrimaryTimeServerImpl(
                serverGuid,
                std::static_pointer_cast<impl::SimpleHandler>(
                    std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        //!Returns list of peers whose local system time is known
        virtual void forceTimeResync() = 0;

    protected:
        virtual int getCurrentTimeImpl( impl::CurrentTimeHandlerPtr handler ) = 0;
        virtual int forcePrimaryTimeServerImpl( const QnUuid& serverGuid, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractTimeManager> AbstractTimeManagerPtr;

    class AbstractMiscNotificationManager : public QObject
    {
        Q_OBJECT
    public:
    signals:
        void systemIdChangeRequested(const QnUuid& systemId, qint64 sysIdTime, Timestamp tranLogTime);
        void miscDataChanged(const QString& name, const QString& value);
    };

    typedef std::shared_ptr<AbstractMiscNotificationManager> AbstractMiscNotificationManagerPtr;

    class AbstractMiscManager {
    public:
        template<class TargetType, class HandlerType> int changeSystemId(const QnUuid& systemId, qint64 sysIdTime, Timestamp tranLogTime, TargetType *target, HandlerType handler) {
            return changeSystemId(systemId, sysIdTime, tranLogTime, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode changeSystemIdSync(const QnUuid& systemId, qint64 sysIdTime, Timestamp tranLogTime) {
            return impl::doSyncCall<impl::SimpleHandler>(
                [=](const impl::SimpleHandlerPtr &handler) {
                    return this->changeSystemId(systemId, sysIdTime, tranLogTime, handler);
                }
            );
        }

        ErrorCode cleanupDatabaseSync(bool cleanupDbObjects, bool cleanupTransactionLog)
        {
            int(AbstractMiscManager::*fn)(bool, bool, impl::SimpleHandlerPtr) = &AbstractMiscManager::cleanupDatabase;
            return impl::doSyncCall<impl::SimpleHandler>(std::bind(fn, this, cleanupDbObjects, cleanupTransactionLog, std::placeholders::_1));
        }

        template<class TargetType, class HandlerType> int markLicenseOverflow(bool value, qint64 time, TargetType *target, HandlerType handler) {
            return markLicenseOverflow(value, time, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode markLicenseOverflowSync(bool value, qint64 time) {
            int(AbstractMiscManager::*fn)(bool, qint64, impl::SimpleHandlerPtr) = &AbstractMiscManager::markLicenseOverflow;
            return impl::doSyncCall<impl::SimpleHandler>(std::bind(fn, this, value, time, std::placeholders::_1));
        }

        ErrorCode saveMiscParamSync(const ec2::ApiMiscData& param)
        {
            int(AbstractMiscManager::*fn)(const ec2::ApiMiscData&, impl::SimpleHandlerPtr) = &AbstractMiscManager::saveMiscParam;
            return impl::doSyncCall<impl::SimpleHandler>(std::bind(fn, this, param, std::placeholders::_1));
        }

        template<class TargetType, class HandlerType> int saveMiscParam(const ApiMiscData& data, TargetType* target, HandlerType handler)
        {
            return saveMiscParam(data, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType>
        int saveRuntimeInfo(const ApiRuntimeData& data, TargetType* target,
            HandlerType handler)
        {
            return saveRuntimeInfo(data, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                    target, handler)));
        }

        template<class TargetType, class HandlerType>
        int getMisParam(const QByteArray& paramName, TargetType* target, HandlerType handler)
        {
            return getMiscParam(paramName, std::static_pointer_cast<impl::GetMiscParamHandler>(std::make_shared<impl::CustomGetMiscParamHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getMiscParamSync(const QByteArray& paramName, ApiMiscData* const outData)
        {
            return impl::doSyncCall<impl::GetMiscParamHandler>(
                [=](const impl::GetMiscParamHandlerPtr &handler) {
                    return this->getMiscParam(paramName, handler);
                },
                outData);
        }

    protected:
        virtual int changeSystemId(const QnUuid& systemId, qint64 sysIdTime, Timestamp tranLogTime, impl::SimpleHandlerPtr handler) = 0;
        virtual int markLicenseOverflow(bool value, qint64 time, impl::SimpleHandlerPtr handler) = 0;
        virtual int cleanupDatabase(bool cleanupDbObjects, bool cleanupTransactionLog, impl::SimpleHandlerPtr handler) = 0;
        virtual int saveMiscParam(const ec2::ApiMiscData& param, impl::SimpleHandlerPtr handler) = 0;
        virtual int saveRuntimeInfo(const ec2::ApiRuntimeData& data, impl::SimpleHandlerPtr handler) = 0;
        virtual int getMiscParam(const QByteArray& paramName, impl::GetMiscParamHandlerPtr handler) = 0;
    };
    typedef std::shared_ptr<AbstractMiscManager> AbstractMiscManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractECConnection: public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractECConnection() {}

        virtual QnConnectionInfo connectionInfo() const = 0;

        /**
         * Changes url client currently uses to connect to server. Required to handle situations
         * like user password change, server port change or systems merge.
         */
        virtual void updateConnectionUrl(const nx::utils::Url& url) = 0;

        //!Calling this method starts notifications delivery by emitting corresponding signals of corresponding manager
        /*!
            \note Calling entity MUST connect to all interesting signals prior to calling this method so that received data is consistent
        */
        virtual void startReceivingNotifications() = 0;
        virtual void stopReceivingNotifications() = 0;

        virtual void addRemotePeer(const QnUuid& id, const nx::utils::Url& _url) = 0;
        virtual void deleteRemotePeer(const QnUuid& id) = 0;

        virtual Timestamp getTransactionLogTime() const = 0;
        virtual void setTransactionLogTime(Timestamp value) = 0;

        virtual AbstractResourceManagerPtr getResourceManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractMediaServerManagerPtr getMediaServerManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractCameraManagerPtr getCameraManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractLicenseManagerPtr getLicenseManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractBusinessEventManagerPtr getBusinessEventManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractUserManagerPtr getUserManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractLayoutManagerPtr getLayoutManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractLayoutTourManagerPtr getLayoutTourManager(const Qn::UserAccessData& userAccessData) = 0;
        virtual AbstractVideowallManagerPtr getVideowallManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractStoredFileManagerPtr getStoredFileManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractUpdatesManagerPtr getUpdatesManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractMiscManagerPtr getMiscManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractDiscoveryManagerPtr getDiscoveryManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractTimeManagerPtr getTimeManager(const Qn::UserAccessData &userAccessData) = 0;
        virtual AbstractWebPageManagerPtr getWebPageManager(const Qn::UserAccessData &userAccessData) = 0;

        virtual AbstractLicenseNotificationManagerPtr getLicenseNotificationManager() = 0;
        virtual AbstractTimeNotificationManagerPtr getTimeNotificationManager() = 0;
        virtual AbstractResourceNotificationManagerPtr getResourceNotificationManager() = 0;
        virtual AbstractMediaServerNotificationManagerPtr getMediaServerNotificationManager() = 0;
        virtual AbstractCameraNotificationManagerPtr getCameraNotificationManager() = 0;
        virtual AbstractBusinessEventNotificationManagerPtr getBusinessEventNotificationManager() = 0;
        virtual AbstractUserNotificationManagerPtr getUserNotificationManager() = 0;
        virtual AbstractLayoutNotificationManagerPtr getLayoutNotificationManager() = 0;
        virtual AbstractLayoutTourNotificationManagerPtr getLayoutTourNotificationManager() = 0;
        virtual AbstractWebPageNotificationManagerPtr getWebPageNotificationManager() = 0;
        virtual AbstractDiscoveryNotificationManagerPtr getDiscoveryNotificationManager() = 0;
        virtual AbstractMiscNotificationManagerPtr getMiscNotificationManager() = 0;
        virtual AbstractUpdatesNotificationManagerPtr getUpdatesNotificationManager() = 0;
        virtual AbstractStoredFileNotificationManagerPtr getStoredFileNotificationManager() = 0;
        virtual AbstractVideowallNotificationManagerPtr getVideowallNotificationManager() = 0;

        virtual QnCommonModule* commonModule() const = 0;

        virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const = 0;
        virtual TransactionMessageBusAdapter* messageBus() const = 0;

        virtual ECConnectionNotificationManager* notificationManager()
        {
            NX_ASSERT(0);
            return nullptr;
        }

        /*!
            \param handler Functor with params: (requestID, ErrorCode, QByteArray dbFile)
        */
        template<class TargetType, class HandlerType> int dumpDatabaseAsync( TargetType* target, HandlerType handler ) {
            return dumpDatabaseAsync( std::static_pointer_cast<impl::DumpDatabaseHandler>(
                std::make_shared<impl::CustomDumpDatabaseHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (requestID, ErrorCode)
        */
        template<class TargetType, class HandlerType> int dumpDatabaseToFileAsync( const QString& dumpFilePath, TargetType* target, HandlerType handler ) {
            return dumpDatabaseToFileAsync( std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(dumpFilePath, target, handler)) );
        }
        ErrorCode dumpDatabaseToFileSync( const QString& dumpFilePath ) {
            int(AbstractECConnection::*fn)(const QString&, impl::SimpleHandlerPtr) = &AbstractECConnection::dumpDatabaseToFileAsync;
            return impl::doSyncCall<impl::SimpleHandler>(std::bind(fn, this, dumpFilePath, std::placeholders::_1));
        }
        /*!
            \param handler Functor with params: (requestID, ErrorCode)
        */
        template<class TargetType, class HandlerType> int restoreDatabaseAsync( const ec2::ApiDatabaseDumpData& data, TargetType* target, HandlerType handler ) {
            return restoreDatabaseAsync( data,
                std::static_pointer_cast<impl::SimpleHandler>(
                    std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        //!Cancel running async request
        /*!
            \warning Request handler may still be called after return of this method, since request could already have been completed and resulte posted to handler
        */
        //virtual void cancelRequest( int requestID ) = 0;
    signals:
        //!Delivers all resources found in Server
        /*!
            This signal is emitted after starting notifications delivery by call to \a AbstractECConnection::startReceivingNotifications
                if full synchronization is requested
            \param resTypes
            \param resList All resources (servers, cameras, users, layouts)
            \param kvPairs Parameters of resources
            \param licenses
            \param cameraHistoryItems
        */
        void initNotification(const ec2::ApiFullInfoData& fullData);
        void runtimeInfoChanged(const ec2::ApiRuntimeData& runtimeInfo);

        void reverseConnectionRequested(const ec2::ApiReverseConnectionData& reverseConnetionData);

        void remotePeerFound(QnUuid data, Qn::PeerType peerType);
        void remotePeerLost(QnUuid data, Qn::PeerType peerType);
        void remotePeerUnauthorized(const QnUuid& id);

        void settingsChanged(ec2::ApiResourceParamDataList settings);

        void databaseDumped();

    protected:
        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) = 0;
        virtual int dumpDatabaseToFileAsync( const QString& dumpFilePath, impl::SimpleHandlerPtr handler ) = 0;
        virtual int restoreDatabaseAsync( const ec2::ApiDatabaseDumpData& data, impl::SimpleHandlerPtr handler ) = 0;
    };

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractECConnectionFactory: public QObject
    {
        Q_OBJECT

    public:
        AbstractECConnectionFactory()
        {}
        virtual ~AbstractECConnectionFactory() {}

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int testConnection( const nx::utils::Url& addr, TargetType* target, HandlerType handler ) {
            return testConnectionAsync( addr, std::static_pointer_cast<impl::TestConnectionHandler>(std::make_shared<impl::CustomTestConnectionHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param addr Empty url designates local Server ("local" means dll linked to executable, not Server running on local host)
            \param handler Functor with params: (ErrorCode, AbstractECConnectionPtr)
        */
        template<class TargetType, class HandlerType> int connect( const nx::utils::Url& addr, const ApiClientInfoData& clientInfo,
                                                                   TargetType* target, HandlerType handler ) {
            auto cch = std::make_shared<impl::CustomConnectHandler<TargetType, HandlerType>>(target, handler);
            return connectAsync(addr, clientInfo, std::static_pointer_cast<impl::ConnectHandler>(cch));
        }
        ErrorCode connectSync( const nx::utils::Url& addr, const ApiClientInfoData& clientInfo, AbstractECConnectionPtr* const connection ) {
            auto call = std::bind(std::mem_fn(&AbstractECConnectionFactory::connectAsync), this, addr, clientInfo, std::placeholders::_1);
            return impl::doSyncCall<impl::ConnectHandler>(call, connection);
        }

        virtual void registerRestHandlers( QnRestProcessorPool* const restProcessorPool ) = 0;
        virtual void registerTransactionListener(QnHttpConnectionListener* httpConnectionListener) = 0;
        virtual void setConfParams( std::map<QString, QVariant> confParams ) = 0;
        virtual TransactionMessageBusAdapter* messageBus() const = 0;
        virtual QnDistributedMutexManager* distributedMutex() const = 0;
        virtual TimeSynchronizationManager* timeSyncManager() const = 0;
    protected:
        virtual int testConnectionAsync( const nx::utils::Url& addr, impl::TestConnectionHandlerPtr handler ) = 0;
        virtual int connectAsync( const nx::utils::Url& addr, const ApiClientInfoData& clientInfo,
                                  impl::ConnectHandlerPtr handler ) = 0;
    };
}

Q_DECLARE_METATYPE(ec2::QnPeerTimeInfo);
Q_DECLARE_METATYPE(ec2::QnPeerTimeInfoList);

#endif  //EC_API_H
