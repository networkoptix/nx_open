#ifndef EC_API_H
#define EC_API_H

#include <algorithm>
#include <functional>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QList>
#include <QtCore/QUrl>

#include "network/module_information.h"

#include <api/model/connection_info.h>
#include <api/model/email_attachment.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/videowall_control_message.h>

#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>
#include <nx_ec/data/api_resource_data.h>
#include <nx_ec/data/api_email_data.h>
#include <nx_ec/data/api_server_alive_data.h>
#include <nx_ec/data/api_time_data.h>
#include <nx_ec/data/api_license_overflow_data.h>
#include <nx_ec/data/api_discovery_data.h>
#include <nx_ec/data/api_camera_history_data.h>
#include <nx_ec/data/api_reverse_connection_data.h>
#include <nx_ec/data/api_client_info_data.h>
#include <nx_ec/data/api_camera_attributes_data.h>
#include <nx_ec/data/api_media_server_data.h>

#include "ec_api_fwd.h"

class QnRestProcessorPool;
class QnHttpConnectionListener;
struct QnModuleInformation;

//!Contains API classes for the new Server
/*!
    TODO describe all API classes
    \note All methods are thread-safe
*/
namespace ec2
{
    struct QnPeerTimeInfo {

        QnPeerTimeInfo():
            time(0){}
        QnPeerTimeInfo(QnUuid peerId, qint64 time):
            peerId(peerId), time(time){}

        QnUuid peerId;

        /** Peer system time (UTC, millis from epoch) */
        qint64 time;
    };

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractResourceManager
    :
        public QObject
    {
        Q_OBJECT

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
        template<class TargetType, class HandlerType>
        int setResourceStatusLocal( const QnUuid& resourceId, Qn::ResourceStatus status, TargetType* target, HandlerType handler ) {
            return setResourceStatusLocal(resourceId, status, std::static_pointer_cast<impl::SetResourceStatusHandler>(std::make_shared<impl::CustomSetResourceStatusHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode setResourceStatusSync( const QnUuid& id, Qn::ResourceStatus status) {
            QnUuid rezId;
            int(AbstractResourceManager::*fn)(const QnUuid&, Qn::ResourceStatus, impl::SetResourceStatusHandlerPtr) = &AbstractResourceManager::setResourceStatus;
            return impl::doSyncCall<impl::SetResourceStatusHandler>( std::bind(fn, this, id, status, std::placeholders::_1), &rezId );
        }
        ErrorCode setResourceStatusLocalSync( const QnUuid& id, Qn::ResourceStatus status) {
            QnUuid rezId;
            int(AbstractResourceManager::*fn)(const QnUuid&, Qn::ResourceStatus, impl::SetResourceStatusHandlerPtr) = &AbstractResourceManager::setResourceStatusLocal;
            return impl::doSyncCall<impl::SetResourceStatusHandler>( std::bind(fn, this, id, status, std::placeholders::_1), &rezId );
        }

        /*!
            \param handler Functor with params: (ErrorCode, const ApiResourceParamWithRefDataList&)
        */
        template<class TargetType, class HandlerType>
        int getKvPairs( const QnUuid& resourceId, TargetType* target, HandlerType handler ) {
            return getKvPairs( resourceId, std::static_pointer_cast<impl::GetKvPairsHandler>(std::make_shared<impl::CustomGetKvPairsHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getKvPairsSync(const QnUuid& resourceId, ApiResourceParamWithRefDataList* const outData) {
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

        ErrorCode getStatusListSync(const QnUuid& resourceId, ApiResourceStatusDataList* const outData) {
            return impl::doSyncCall<impl::GetStatusListHandler>(
                [=](const impl::GetStatusListHandlerPtr &handler) {
                    return this->getStatusList(resourceId, handler);
                },
                outData
            );
        }

        /*!
        template<class TargetType, class HandlerType> int setResourceDisabled( const QnUuid& resourceId, bool disabled, TargetType* target, HandlerType handler ) {
            return setResourceDisabled( resourceId, disabled, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        */


        /*!
            \param handler Functor with params: (ErrorCode, const ApiResourceParamWithRefDataList&)
        */
        template<class TargetType, class HandlerType>
        int save(const ec2::ApiResourceParamWithRefDataList& kvPairs, TargetType* target, HandlerType handler ) {
            return save(kvPairs, std::static_pointer_cast<impl::SaveKvPairsHandler>(std::make_shared<impl::CustomSaveKvPairsHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode saveSync(const ec2::ApiResourceParamWithRefDataList& kvPairs, ApiResourceParamWithRefDataList* const outData) {
            return impl::doSyncCall<impl::SaveKvPairsHandler>(
                [=](const impl::SaveKvPairsHandlerPtr &handler) {
                    return this->save(kvPairs, handler);
                },
                outData
            );
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

    signals:
        void statusChanged( const QnUuid& resourceId, Qn::ResourceStatus status );
        void resourceChanged( const QnResourcePtr& resource );
        void resourceParamChanged( const ApiResourceParamWithRefData& param );
        void resourceRemoved( const QnUuid& resourceId );

    protected:
        virtual int getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) = 0;
        virtual int setResourceStatus( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler ) = 0;
        virtual int setResourceStatusLocal( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler ) = 0;
        virtual int getKvPairs( const QnUuid &resourceId, impl::GetKvPairsHandlerPtr handler ) = 0;
        virtual int getStatusList( const QnUuid &resourceId, impl::GetStatusListHandlerPtr handler ) = 0;
        virtual int save(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) = 0;
        virtual int removeParams(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) = 0;
        virtual int remove( const QnUuid& resource, impl::SimpleHandlerPtr handler ) = 0;
        virtual int remove( const QVector<QnUuid>& resourceList, impl::SimpleHandlerPtr handler ) = 0;
    };


    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractLicenseManager
    :
        public QObject
    {
        Q_OBJECT

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


    signals:
        void licenseChanged(QnLicensePtr license);
        void licenseRemoved(QnLicensePtr license);

    protected:
        virtual int getLicenses( impl::GetLicensesHandlerPtr handler ) = 0;
        virtual int addLicenses( const QList<QnLicensePtr>& licenses, impl::SimpleHandlerPtr handler ) = 0;
        virtual int removeLicense( const QnLicensePtr& license, impl::SimpleHandlerPtr handler ) = 0;
    };


    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractBusinessEventManager
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractBusinessEventManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnBusinessEventRuleList&)
        */
        template<class TargetType, class HandlerType> int getBusinessRules( TargetType* target, HandlerType handler ) {
            return getBusinessRules( std::static_pointer_cast<impl::GetBusinessRulesHandler>(
                std::make_shared<impl::CustomGetBusinessRulesHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getBusinessRulesSync(QnBusinessEventRuleList* const businessEventList ) {
            int(AbstractBusinessEventManager::*fn)(impl::GetBusinessRulesHandlerPtr) = &AbstractBusinessEventManager::getBusinessRules;
            return impl::doSyncCall<impl::GetBusinessRulesHandler>( std::bind(fn, this, std::placeholders::_1), businessEventList );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save( const QnBusinessEventRulePtr& rule, TargetType* target, HandlerType handler ) {
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
        template<class TargetType, class HandlerType> int broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, TargetType* target, HandlerType handler ) {
            return broadcastBusinessAction( businessAction, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        template<class TargetType, class HandlerType> int sendBusinessAction( const QnAbstractBusinessActionPtr& businessAction, const QnUuid& dstPeer, TargetType* target, HandlerType handler ) {
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

    signals:
        void addedOrUpdated( QnBusinessEventRulePtr businessRule );
        void removed( QnUuid id );
        void businessActionBroadcasted( const QnAbstractBusinessActionPtr& businessAction );
        void businessRuleReset( const ec2::ApiBusinessRuleDataList& rules );
        void gotBroadcastAction(const QnAbstractBusinessActionPtr& action);
        void execBusinessAction(const QnAbstractBusinessActionPtr& action);

    private:
        virtual int getBusinessRules( impl::GetBusinessRulesHandlerPtr handler ) = 0;
        virtual int save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler ) = 0;
        virtual int deleteRule( QnUuid ruleId, impl::SimpleHandlerPtr handler ) = 0;
        virtual int broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) = 0;
        virtual int sendBusinessAction( const QnAbstractBusinessActionPtr& businessAction, const QnUuid& id, impl::SimpleHandlerPtr handler ) = 0;
        virtual int resetBusinessRules( impl::SimpleHandlerPtr handler ) = 0;
    };

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractStoredFileManager
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractStoredFileManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QByteArray&)
        */
        template<class TargetType, class HandlerType> int getStoredFile( const QString& filename, TargetType* target, HandlerType handler ) {
            return getStoredFile( filename, std::static_pointer_cast<impl::GetStoredFileHandler>(
                std::make_shared<impl::CustomGetStoredFileHandler<TargetType, HandlerType>>(target, handler)) );
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

    signals:
        void added( QString filename );
        void updated( QString filename );
        void removed( QString filename );

    protected:
        virtual int getStoredFile( const QString& filename, impl::GetStoredFileHandlerPtr handler ) = 0;
        virtual int addStoredFile( const QString& filename, const QByteArray& data, impl::SimpleHandlerPtr handler ) = 0;
        virtual int deleteStoredFile( const QString& filename, impl::SimpleHandlerPtr handler ) = 0;
        virtual int listDirectory( const QString& folderName, impl::ListDirectoryHandlerPtr handler ) = 0;
    };


    class AbstractUpdatesManager : public QObject {
        Q_OBJECT
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

        template<class TargetType, class HandlerType> int installUpdate(const QString &updateId, const QnPeerSet &peers, TargetType *target, HandlerType handler) {
            return installUpdate(updateId, peers, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

    signals:
        void updateChunkReceived(const QString &updateId, const QByteArray &data, qint64 offset);
        void updateUploadProgress(const QString &updateId, const QnUuid &peerId, int chunks);
        void updateInstallationRequested(const QString &updateId);

    protected:
        virtual int sendUpdatePackageChunk(const QString &updateId, const QByteArray &data, qint64 offset, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) = 0;
        virtual int sendUpdateUploadResponce(const QString &updateId, const QnUuid &peerId, int chunks, impl::SimpleHandlerPtr handler) = 0;
        virtual int installUpdate(const QString &updateId, const QnPeerSet &peers, impl::SimpleHandlerPtr handler) = 0;
    };


    class AbstractDiscoveryManager : public QObject {
        Q_OBJECT
    public:
        template<class TargetType, class HandlerType> int discoverPeer(const QUrl &url, TargetType *target, HandlerType handler) {
            return discoverPeer(url, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType> int addDiscoveryInformation(const QnUuid &id, const QUrl&url, bool ignore, TargetType *target, HandlerType handler) {
            return addDiscoveryInformation(id, url, ignore, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType> int removeDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, TargetType *target, HandlerType handler) {
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

        template<class TargetType, class HandlerType> int sendDiscoveredServer(const ApiDiscoveredServerData &discoveredServer, TargetType *target, HandlerType handler) {
            return sendDiscoveredServer(discoveredServer, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType> int sendDiscoveredServersList(const ApiDiscoveredServerDataList &discoveredServersList, TargetType *target, HandlerType handler) {
            return sendDiscoveredServersList(discoveredServersList, std::static_pointer_cast<impl::SimpleHandler>(
                 std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

    signals:
        void peerDiscoveryRequested(const QUrl &url);
        void discoveryInformationChanged(const ApiDiscoveryData &data, bool addInformation);
        void discoveredServerChanged(const ApiDiscoveredServerData &discoveredServer);
        void gotInitialDiscoveredServers(const ApiDiscoveredServerDataList &discoveredServers);

    protected:
        virtual int discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler) = 0;
        virtual int addDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) = 0;
        virtual int removeDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) = 0;
        virtual int getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler) = 0;
        virtual int sendDiscoveredServer(const ApiDiscoveredServerData &discoveredServer, impl::SimpleHandlerPtr handler) = 0;
        virtual int sendDiscoveredServersList(const ApiDiscoveredServerDataList &discoveredServersList, impl::SimpleHandlerPtr handler) = 0;
    };
    typedef std::shared_ptr<AbstractDiscoveryManager> AbstractDiscoveryManagerPtr;

    class AbstractTimeManager : public QObject {
        Q_OBJECT
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
        virtual QnPeerTimeInfoList getPeerTimeInfoList() const = 0;
        virtual void forceTimeResync() = 0;

    signals:
        //!Emitted when there is ambiguity while choosing primary time server automatically
        /*!
            User SHOULD call \a AbstractTimeManager::forcePrimaryTimeServer to set primary time server manually.
            This signal is emitted periodically until ambiguity in choosing primary time server has been resolved (by user or automatically)
        */
        void timeServerSelectionRequired();
        //!Emitted when synchronized time has been changed
        void timeChanged( qint64 syncTime );
        //!Emitted when peer \a peerId local time has changed
        /*!
            \param peerId
            \param syncTime Synchronized time (UTC, millis from epoch) corresponding to \a peerLocalTime
            \param peerLocalTime Peer local time (UTC, millis from epoch)
        */
        void peerTimeChanged(const QnUuid &peerId, qint64 syncTime, qint64 peerLocalTime);

    protected:
        virtual int getCurrentTimeImpl( impl::CurrentTimeHandlerPtr handler ) = 0;
        virtual int forcePrimaryTimeServerImpl( const QnUuid& serverGuid, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractTimeManager> AbstractTimeManagerPtr;

    class AbstractMiscManager : public QObject {
        Q_OBJECT
    public:
        template<class TargetType, class HandlerType> int changeSystemName(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime, TargetType *target, HandlerType handler) {
            return changeSystemName(systemName, sysIdTime, tranLogTime, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode changeSystemNameSync(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime) {
            return impl::doSyncCall<impl::SimpleHandler>(
                [=](const impl::SimpleHandlerPtr &handler) {
                    return this->changeSystemName(systemName, sysIdTime, tranLogTime, handler);
                }
            );
        }

        template<class TargetType, class HandlerType> int markLicenseOverflow(bool value, qint64 time, TargetType *target, HandlerType handler) {
            return markLicenseOverflow(value, time, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode markLicenseOverflowSync(bool value, qint64 time) {
            int(AbstractMiscManager::*fn)(bool, qint64, impl::SimpleHandlerPtr) = &AbstractMiscManager::markLicenseOverflow;
            return impl::doSyncCall<impl::SimpleHandler>(std::bind(fn, this, value, time, std::placeholders::_1));
        }


    signals:
        void systemNameChangeRequested(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime);

    protected:
        virtual int changeSystemName(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime, impl::SimpleHandlerPtr handler) = 0;
        virtual int markLicenseOverflow(bool value, qint64 time, impl::SimpleHandlerPtr handler) = 0;
    };
    typedef std::shared_ptr<AbstractMiscManager> AbstractMiscManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractECConnection
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractECConnection() {}

        virtual QnConnectionInfo connectionInfo() const = 0;

        virtual QString authInfo() const = 0;
        //!Calling this method starts notifications delivery by emitting corresponding signals of corresponding manager
        /*!
            \note Calling entity MUST connect to all interesting signals prior to calling this method so that received data is consistent
        */
        virtual void startReceivingNotifications() = 0;
        virtual void stopReceivingNotifications() = 0;

        virtual void addRemotePeer(const QUrl& url) = 0;
        virtual void deleteRemotePeer(const QUrl& url) = 0;
        virtual void sendRuntimeData(const ec2::ApiRuntimeData &data) = 0;

        virtual qint64 getTransactionLogTime() const = 0;
        virtual void setTransactionLogTime(qint64 value) = 0;

        virtual AbstractResourceManagerPtr getResourceManager() = 0;
        virtual AbstractMediaServerManagerPtr getMediaServerManager() = 0;
        virtual AbstractCameraManagerPtr getCameraManager() = 0;
        virtual AbstractLicenseManagerPtr getLicenseManager() = 0;
        virtual AbstractBusinessEventManagerPtr getBusinessEventManager() = 0;
        virtual AbstractUserManagerPtr getUserManager() = 0;
        virtual AbstractLayoutManagerPtr getLayoutManager() = 0;
        virtual AbstractVideowallManagerPtr getVideowallManager() = 0;
        virtual AbstractStoredFileManagerPtr getStoredFileManager() = 0;
        virtual AbstractUpdatesManagerPtr getUpdatesManager() = 0;
        virtual AbstractMiscManagerPtr getMiscManager() = 0;
        virtual AbstractDiscoveryManagerPtr getDiscoveryManager() = 0;
        virtual AbstractTimeManagerPtr getTimeManager() = 0;
        virtual AbstractWebPageManagerPtr getWebPageManager() = 0;

        virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const = 0;

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

        void remotePeerFound(const ec2::ApiPeerAliveData& data);
        void remotePeerLost(const ec2::ApiPeerAliveData& data);
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
    class AbstractECConnectionFactory
    :
        public QObject
    {
        Q_OBJECT

    public:
        AbstractECConnectionFactory():
            m_compatibilityMode(false)
        {}
        virtual ~AbstractECConnectionFactory() {}

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int testConnection( const QUrl& addr, TargetType* target, HandlerType handler ) {
            return testConnectionAsync( addr, std::static_pointer_cast<impl::TestConnectionHandler>(std::make_shared<impl::CustomTestConnectionHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param addr Empty url designates local Server ("local" means dll linked to executable, not Server running on local host)
            \param handler Functor with params: (ErrorCode, AbstractECConnectionPtr)
        */
        template<class TargetType, class HandlerType> int connect( const QUrl& addr, const ApiClientInfoData& clientInfo,
                                                                   TargetType* target, HandlerType handler ) {
            auto cch = std::make_shared<impl::CustomConnectHandler<TargetType, HandlerType>>(target, handler);
            return connectAsync(addr, clientInfo, std::static_pointer_cast<impl::ConnectHandler>(cch));
        }
        ErrorCode connectSync( const QUrl& addr, const ApiClientInfoData& clientInfo, AbstractECConnectionPtr* const connection ) {
            auto call = std::bind(std::mem_fn(&AbstractECConnectionFactory::connectAsync), this, addr, clientInfo, std::placeholders::_1);
            return impl::doSyncCall<impl::ConnectHandler>(call, connection);
        }

        virtual void registerRestHandlers( QnRestProcessorPool* const restProcessorPool ) = 0;
        virtual void registerTransactionListener(QnHttpConnectionListener* httpConnectionListener) = 0;
        virtual void setConfParams( std::map<QString, QVariant> confParams ) = 0;

        /**
        * \returns                         Whether this connection factory is working in compatibility mode.
        *                                  In this mode all clients are supported regardless of customization.
        */
        bool isCompatibilityMode() const {
            return m_compatibilityMode;
        }

        //! \param compatibilityMode         New compatibility mode state.
        void setCompatibilityMode(bool compatibilityMode) {
            m_compatibilityMode = compatibilityMode;
        }
    protected:
        virtual int testConnectionAsync( const QUrl& addr, impl::TestConnectionHandlerPtr handler ) = 0;
        virtual int connectAsync( const QUrl& addr, const ApiClientInfoData& clientInfo,
                                  impl::ConnectHandlerPtr handler ) = 0;

    private:
        bool m_compatibilityMode;
    };
}

Q_DECLARE_METATYPE(ec2::QnPeerTimeInfo);
Q_DECLARE_METATYPE(ec2::QnPeerTimeInfoList);

#endif  //EC_API_H
