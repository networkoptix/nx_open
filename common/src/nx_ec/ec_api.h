
#ifndef EC_API_H
#define EC_API_H

#include <algorithm>
#include <functional>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QUrl>

#include "api/model/connection_info.h"
#include "api/model/email_attachment.h"
#include "data/ec2_runtime_info.h"
#include "impl/ec_api_impl.h"
#include "impl/sync_handler.h"
#include "rest/server/rest_connection_processor.h"
#include "network/universal_tcp_listener.h"

//!Contains API classes for the new enterprise controller
/*!
    TODO describe all API classes
    \note All methods are thread-safe
*/
namespace ec2
{

    struct ServerInfo
    {
        QList<QByteArray> mainHardwareIds;
        QList<QByteArray> compatibleHardwareIds;

        /* QByteArray hardwareId1;
        QByteArray oldHardwareId;
        QByteArray hardwareId2;
        QByteArray hardwareId3; */

        QString publicIp;
        QString systemName;
        QString sessionKey;
        bool allowCameraChanges;
    };

    struct QnFullResourceData
    {
        ServerInfo serverInfo;
        QnResourceTypeList resTypes;
        QnResourceList resources;
        QnBusinessEventRuleList bRules;
        QnCameraHistoryList cameraHistory;
        QnLicenseList licenses;
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
        template<class TargetType, class HandlerType> int getResourceTypes( TargetType* target, HandlerType handler ) {
            return getResourceTypes( std::static_pointer_cast<impl::GetResourceTypesHandler>(std::make_shared<impl::CustomGetResourceTypesHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode getResourceTypesSync( QnResourceTypeList* const resTypeList ) {
            using namespace std::placeholders;
            int(AbstractResourceManager::*fn)(impl::GetResourceTypesHandlerPtr) = &AbstractResourceManager::getResourceTypes;
            return impl::doSyncCall<impl::GetResourceTypesHandler>( std::bind(fn, this, _1), resTypeList );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int setResourceStatus( const QnId& resourceId, QnResource::Status status, TargetType* target, HandlerType handler ) {
            return setResourceStatus(resourceId, status, std::static_pointer_cast<impl::SetResourceStatusHandler>(std::make_shared<impl::CustomSetResourceStatusHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode setResourceStatusSync( const QnId& id, QnResource::Status status) {
            using namespace std::placeholders;
            QnId rezId;
            int(AbstractResourceManager::*fn)(const QnId&, QnResource::Status, impl::SetResourceStatusHandlerPtr) = &AbstractResourceManager::setResourceStatus;
            return impl::doSyncCall<impl::SetResourceStatusHandler>( std::bind(fn, this, id, status, _1), &rezId );
        }

        /*!
            \param handler Functor with params: (ErrorCode, const QnKvPairListsById&)
        */
        template<class TargetType, class HandlerType> int getKvPairs( const QnId& resourceId, TargetType* target, HandlerType handler ) {
            return getKvPairs( resourceId, std::static_pointer_cast<impl::GetKvPairsHandler>(std::make_shared<impl::CustomGetKvPairsHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int setResourceDisabled( const QnId& resourceId, bool disabled, TargetType* target, HandlerType handler ) {
            return setResourceDisabled( resourceId, disabled, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        //!Saves changes to common resource's properties (e.g., name). Accepts any resource
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save( const QnResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return save( resource, std::static_pointer_cast<impl::SaveResourceHandler>(std::make_shared<impl::CustomSaveResourceHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QnKvPairListsById&)
        */
        template<class TargetType, class HandlerType> int save( const QnId& resourceId, const QnKvPairList& kvPairs, TargetType* target, HandlerType handler ) {
            return save(resourceId, kvPairs,  std::static_pointer_cast<impl::SaveKvPairsHandler>(std::make_shared<impl::CustomSaveKvPairsHandler<TargetType, HandlerType>>(target, handler)) );
        }
        //!Convenient method to remove resource of any type
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int remove( const QnId& id, TargetType* target, HandlerType handler ) {
            return remove( id, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void statusChanged( const QnId& resourceId, QnResource::Status status );
        void disabledChanged( const QnId& resourceId, bool disabled );
        void resourceChanged( const QnResourcePtr& resource );
        void resourceParamsChanged( const QnId& resourceId, const QnKvPairList& kvPairs );
        void resourceRemoved( const QnId& resourceId );

    protected:
        virtual int getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) = 0;
        virtual int setResourceStatus( const QnId& resourceId, QnResource::Status status, impl::SetResourceStatusHandlerPtr handler ) = 0;
        virtual int setResourceDisabled( const QnId& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler ) = 0;
        virtual int getKvPairs( const QnId &resourceId, impl::GetKvPairsHandlerPtr handler ) = 0;
        virtual int save( const QnResourcePtr &resource, impl::SaveResourceHandlerPtr handler ) = 0;
        virtual int save( const QnId& resourceId, const QnKvPairList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) = 0;
        virtual int remove( const QnId& resource, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractResourceManager> AbstractResourceManagerPtr;


    class AbstractMediaServerManager
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractMediaServerManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnMediaServerResourceList& servers)
        */
        template<class TargetType, class HandlerType> int getServers( TargetType* target, HandlerType handler ) {
            return getServers( std::static_pointer_cast<impl::GetServersHandler>(std::make_shared<impl::CustomGetServersHandler<TargetType, HandlerType>>(target, handler)) );
        }
        
        ErrorCode getServersSync(QnMediaServerResourceList* const serverList ) {
            using namespace std::placeholders;
            int(AbstractMediaServerManager::*fn)(impl::GetServersHandlerPtr) = &AbstractMediaServerManager::getServers;
            return impl::doSyncCall<impl::GetServersHandler>( std::bind(fn, this, _1), serverList );
        }

        /*!
            \param handler Functor with params: (ErrorCode, const QnMediaServerResourcePtr& servers)
        */
        template<class TargetType, class HandlerType> int save( const QnMediaServerResourcePtr& mserver, TargetType* target, HandlerType handler ) {
            return save( mserver, std::static_pointer_cast<impl::SaveServerHandler>(std::make_shared<impl::CustomSaveServerHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode saveSync( const QnMediaServerResourcePtr& serverRes, QnMediaServerResourcePtr* const server ) {
            using namespace std::placeholders;
            int(AbstractMediaServerManager::*fn)(const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr) = &AbstractMediaServerManager::save;
            return impl::doSyncCall<impl::SaveServerHandler>( std::bind(fn, this, serverRes, _1), server );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int remove( const QnId& id, TargetType* target, HandlerType handler ) {
            return remove( id, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void addedOrUpdated( QnMediaServerResourcePtr camera );
        void removed( QnId id );
    protected:
        virtual int getServers( impl::GetServersHandlerPtr handler ) = 0;
        virtual int save( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) = 0;
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractMediaServerManager> AbstractMediaServerManagerPtr;


    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractCameraManager: public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractCameraManager() {}

        /*!
            Returns list of all available cameras. 
            \todo is it really needed?
            \param handler Functor with params: (ErrorCode, const QnVirtualCameraResourceList& cameras)
        */
        template<class TargetType, class HandlerType> int addCamera( const QnVirtualCameraResourcePtr& camRes, TargetType* target, HandlerType handler ) {
            return addCamera( camRes, std::static_pointer_cast<impl::AddCameraHandler>(std::make_shared<impl::CustomAddCameraHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode addCameraSync( const QnVirtualCameraResourcePtr& camRes, QnVirtualCameraResourceList* const cameras = 0) {
            using namespace std::placeholders;
            int(AbstractCameraManager::*fn)(const QnVirtualCameraResourcePtr&, impl::AddCameraHandlerPtr) = &AbstractCameraManager::addCamera;
            return impl::doSyncCall<impl::AddCameraHandler>( std::bind(fn, this, camRes, _1), cameras );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, TargetType* target, HandlerType handler ) {
            return addCameraHistoryItem( std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode addCameraHistoryItemSync( const QnCameraHistoryItem& historyItem) {
            using namespace std::placeholders;
            int(AbstractCameraManager::*fn)(const QnCameraHistoryItem&, impl::SimpleHandlerPtr) = &AbstractCameraManager::addCameraHistoryItem;
            return impl::doSyncCall<impl::SimpleHandler>( std::bind(fn, this, historyItem, _1));
        }

        /*!
            \param handler Functor with params: (ErrorCode, const QnVirtualCameraResourceList& cameras)
        */
        template<class TargetType, class HandlerType> int getCameras( const QnId& mediaServerId, TargetType* target, HandlerType handler ) {
            return getCameras( std::static_pointer_cast<impl::GetCamerasHandler>(std::make_shared<impl::CustomGetCamerasHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getCamerasSync(const QnId& mServerId, QnVirtualCameraResourceList* const cameraList ) {
            using namespace std::placeholders;
            int(AbstractCameraManager::*fn)(const QnId&, impl::GetCamerasHandlerPtr) = &AbstractCameraManager::getCameras;
            return impl::doSyncCall<impl::GetCamerasHandler>( std::bind(fn, this, mServerId, _1), cameraList );
        }


        /*!
            \param handler Functor with params: (ErrorCode, const QnCameraHistoryList& cameras)
        */
        template<class TargetType, class HandlerType> int getCameraHistoryList( TargetType* target, HandlerType handler ) {
            return getCameraHistoryList( std::static_pointer_cast<impl::GetCamerasHistoryHandler>(std::make_shared<impl::CustomGetCamerasHistoryHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getCameraHistoryListSync(QnCameraHistoryList* const cameraHistoryList ) {
            using namespace std::placeholders;
            int(AbstractCameraManager::*fn)(impl::GetCamerasHistoryHandlerPtr) = &AbstractCameraManager::getCameraHistoryList;
            return impl::doSyncCall<impl::GetCamerasHistoryHandler>( std::bind(fn, this,  _1), cameraHistoryList );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save( const QnVirtualCameraResourceList& cameras, TargetType* target, HandlerType handler ) {
            return save( cameras, std::static_pointer_cast<impl::AddCameraHandler>(std::make_shared<impl::CustomAddCameraHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int remove( const QnId& id, TargetType* target, HandlerType handler ) {
            return remove(id, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void cameraAddedOrUpdated( QnVirtualCameraResourcePtr camera );
        void cameraHistoryChanged( QnCameraHistoryItemPtr cameraHistory );
        void cameraRemoved( QnId id );

    protected:
        virtual int addCamera( const QnVirtualCameraResourcePtr&, impl::AddCameraHandlerPtr handler ) = 0;
        virtual int addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler ) = 0;
        virtual int getCameras( const QnId& mediaServerId, impl::GetCamerasHandlerPtr handler ) = 0;
        virtual int getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler ) = 0;
        virtual int save( const QnVirtualCameraResourceList& cameras, impl::AddCameraHandlerPtr handler ) = 0;
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractCameraManager> AbstractCameraManagerPtr;

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
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int addLicenses( const QList<QnLicensePtr>& licenses, TargetType* target, HandlerType handler ) {
            return addLicenses( licenses, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void licenseChanged(QnLicensePtr license);

    protected:
        virtual int getLicenses( impl::GetLicensesHandlerPtr handler ) = 0;
        virtual int addLicenses( const QList<QnLicensePtr>& licenses, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractLicenseManager> AbstractLicenseManagerPtr;

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
            using namespace std::placeholders;
            int(AbstractBusinessEventManager::*fn)(impl::GetBusinessRulesHandlerPtr) = &AbstractBusinessEventManager::getBusinessRules;
            return impl::doSyncCall<impl::GetBusinessRulesHandler>( std::bind(fn, this, _1), businessEventList );
        }

        //!Test if email settings are valid
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int testEmailSettings( const QnKvPairList& settings, TargetType* target, HandlerType handler ) {
            return testEmailSettings( settings, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param to Destination address list
            \param timeout TODO
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int sendEmail(
            const QStringList& to,
            const QString& subject,
            const QString& message,
            int timeout,
            const QnEmailAttachmentList& attachments,
            TargetType* target, HandlerType handler )
        {
            return sendEmail( to, subject, message, timeout, attachments, 
                std::static_pointer_cast<impl::SimpleHandler>(
                    std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
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
        template<class TargetType, class HandlerType> int deleteRule( QnId ruleId, TargetType* target, HandlerType handler ) {
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
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int resetBusinessRules( TargetType* target, HandlerType handler ) {
            return resetBusinessRules( std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void addedOrUpdated( QnBusinessEventRulePtr businessRule );
        void removed( QnId id );
        void businessActionBroadcasted( const QnAbstractBusinessActionPtr& businessAction );
        void businessRuleReset( const QnBusinessEventRuleList& rules );
        void gotBroadcastAction(const QnAbstractBusinessActionPtr& action);

    private:
        virtual int getBusinessRules( impl::GetBusinessRulesHandlerPtr handler ) = 0;
        virtual int testEmailSettings( const QnKvPairList& settings, impl::SimpleHandlerPtr handler ) = 0;
        virtual int sendEmail(
            const QStringList& to,
            const QString& subject,
            const QString& message,
            int timeout,
            const QnEmailAttachmentList& attachments,
            impl::SimpleHandlerPtr handler ) = 0;
        virtual int save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler ) = 0;
        virtual int deleteRule( QnId ruleId, impl::SimpleHandlerPtr handler ) = 0;
        virtual int broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) = 0;
        virtual int resetBusinessRules( impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractBusinessEventManager> AbstractBusinessEventManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractUserManager
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractUserManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnUserResourceList&)
        */
        template<class TargetType, class HandlerType> int getUsers( QnUserResourceList& users, TargetType* target, HandlerType handler ) {
            return getUsers( getUsers, std::static_pointer_cast<impl::GetUsersHandler>(
                std::make_shared<impl::CustomGetUsersHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getUsersSync(QnUserResourceList* const userList ) {
            using namespace std::placeholders;
            int(AbstractUserManager::*fn)(impl::GetUsersHandlerPtr) = &AbstractUserManager::getUsers;
            return impl::doSyncCall<impl::GetUsersHandler>( std::bind(fn, this, _1), userList );
        }


        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save( const QnUserResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return save( resource, std::static_pointer_cast<impl::AddUserHandler>(
                std::make_shared<impl::CustomAddUserHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int remove( const QnId& id, TargetType* target, HandlerType handler ) {
            return remove( id, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void addedOrUpdated( QnUserResourcePtr camera );
        void removed( QnId id );

    private:
        virtual int getUsers( impl::GetUsersHandlerPtr handler ) = 0;
        virtual int save( const QnUserResourcePtr& resource, impl::AddUserHandlerPtr handler ) = 0;
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractUserManager> AbstractUserManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractLayoutManager
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractLayoutManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnLayoutResourceList&)
        */
        template<class TargetType, class HandlerType> int getLayouts( TargetType* target, HandlerType handler ) {
            return getLayouts( std::static_pointer_cast<impl::GetLayoutsHandler>(
                std::make_shared<impl::CustomGetLayoutsHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save( const QnLayoutResourceList& resources, TargetType* target, HandlerType handler ) {
            return save( resources, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int remove( const QnId& resource, TargetType* target, HandlerType handler ) {
            return remove( resource, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void addedOrUpdated( QnLayoutResourcePtr camera );
        void removed( QnId id );

    protected:
        virtual int getLayouts( impl::GetLayoutsHandlerPtr handler ) = 0;
        virtual int save( const QnLayoutResourceList& resources, impl::SimpleHandlerPtr handler ) = 0;
        virtual int remove( const QnId& resource, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractLayoutManager> AbstractLayoutManagerPtr;

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
    typedef std::shared_ptr<AbstractStoredFileManager> AbstractStoredFileManagerPtr;

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
        //!Calling this method starts notifications delivery by emitting corresponding signals of corresponding manager
        /*!
            \param fullSyncRequired If \a true, \a AbstractECConnection::initNotification signal is delivered before any other signal
            \note Calling entity MUST connect to all interesting signals prior to calling this method so that received data is consistent
        */
        virtual void startReceivingNotifications( bool fullSyncRequired) = 0;
        virtual void addRemotePeer(const QUrl& url, bool isClient) = 0;
        virtual void deleteRemotePeer(const QUrl& url) = 0;

        virtual AbstractResourceManagerPtr getResourceManager() = 0;
        virtual AbstractMediaServerManagerPtr getMediaServerManager() = 0;
        virtual AbstractCameraManagerPtr getCameraManager() = 0;
        virtual AbstractLicenseManagerPtr getLicenseManager() = 0;
        virtual AbstractBusinessEventManagerPtr getBusinessEventManager() = 0;
        virtual AbstractUserManagerPtr getUserManager() = 0;
        virtual AbstractLayoutManagerPtr getLayoutManager() = 0;
        virtual AbstractStoredFileManagerPtr getStoredFileManager() = 0;

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int setPanicMode( Qn::PanicMode value, TargetType* target, HandlerType handler ) {
            return setPanicMode( value,
                std::static_pointer_cast<impl::SimpleHandler>(
                    std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode setPanicModeSync(Qn::PanicMode value) {
            using namespace std::placeholders;
            int(AbstractECConnection::*fn)(Qn::PanicMode, impl::SimpleHandlerPtr) = &AbstractECConnection::setPanicMode;
            return impl::doSyncCall<impl::SimpleHandler>( std::bind(fn, this, value, _1));
        }

        /*!
            \param handler Functor with params: (ErrorCode, qint64)
        */
        template<class TargetType, class HandlerType> int getCurrentTime( TargetType* target, HandlerType handler ) {
            return getCurrentTime( std::static_pointer_cast<impl::CurrentTimeHandler>(
                std::make_shared<impl::CustomCurrentTimeHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getCurrentTimeSync(qint64* const time) {
            using namespace std::placeholders;
            int(AbstractECConnection::*fn)(impl::CurrentTimeHandlerPtr) = &AbstractECConnection::getCurrentTime;
            return impl::doSyncCall<impl::CurrentTimeHandler>( std::bind(fn, this, _1), time );
        }

        /*!
            \param handler Functor with params: (ErrorCode, QByteArray dbFile)
        */
        template<class TargetType, class HandlerType> int dumpDatabaseAsync( TargetType* target, HandlerType handler ) {
            return dumpDatabaseAsync( std::static_pointer_cast<impl::DumpDatabaseHandler>(
                std::make_shared<impl::CustomDumpDatabaseHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int restoreDatabaseAsync( const QByteArray& dbFile, TargetType* target, HandlerType handler ) {
            return restoreDatabaseAsync( dbFile,
                std::static_pointer_cast<impl::SimpleHandler>(
                    std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QnKvPairList&)
            \return Request id
        */
        template<class TargetType, class HandlerType> int getSettingsAsync( TargetType* target, HandlerType handler ) {
            return getSettingsAsync( std::static_pointer_cast<impl::GetSettingsHandler>(
                std::make_shared<impl::CustomGetSettingsHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int saveSettingsAsync( const QnKvPairList& kvPairs, TargetType* target, HandlerType handler ) {
            return saveSettingsAsync( kvPairs, 
                std::static_pointer_cast<impl::SimpleHandler>(
                    std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        //!Cancel running async request
        /*!
            \warning Request handler may still be called after return of this method, since request could already have been completed and resulte posted to handler
        */
        //virtual void cancelRequest( int requestID ) = 0;

    signals:
        //!Delivers all resources found in EC
        /*!
            This signal is emitted after starting notifications delivery by call to \a AbstractECConnection::startReceivingNotifications 
                if full synchronization is requested
            \param resTypes
            \param resList All resources (servers, cameras, users, layouts)
            \param kvPairs Parameters of resources
            \param licenses
            \param cameraHistoryItems
        */
        void initNotification(QnFullResourceData fullData);
        void runtimeInfoChanged(const ec2::QnRuntimeInfo& runtimeInfo);

        void remotePeerFound(QnId id, bool isClient, bool isProxy);
        void remotePeerLost(QnId id, bool isClient, bool isProxy);

        void settingsChanged(QnKvPairList settings);

    protected:
        virtual int setPanicMode( Qn::PanicMode value, impl::SimpleHandlerPtr handler ) = 0;
        virtual int getCurrentTime( impl::CurrentTimeHandlerPtr handler ) = 0;
        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) = 0;
        virtual int restoreDatabaseAsync( const QByteArray& dbFile, impl::SimpleHandlerPtr handler ) = 0;
        virtual int getSettingsAsync( impl::GetSettingsHandlerPtr handler ) = 0;
        virtual int saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) = 0;
    };  

    typedef std::shared_ptr<AbstractECConnection> AbstractECConnectionPtr;

    struct ResourceContext
    {
        QnResourceFactory* resFactory;
        QnResourcePool* pool;
        const QnResourceTypePool* resTypePool;

        ResourceContext()
        :
            resFactory( nullptr ),
            pool( nullptr ),
            resTypePool( nullptr )
        {
        }
        ResourceContext(
            QnResourceFactory* _resFactory,
            QnResourcePool* _pool,
            const QnResourceTypePool* _resTypePool )
        :
            resFactory( _resFactory ),
            pool( _pool ),
            resTypePool( _resTypePool )
        {
        }
    };

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractECConnectionFactory
    {
    public:
        virtual ~AbstractECConnectionFactory() {}

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int testConnection( const QUrl& addr, TargetType* target, HandlerType handler ) {
            return testConnectionAsync( addr, std::static_pointer_cast<impl::TestConnectionHandler>(std::make_shared<impl::CustomTestConnectionHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param addr Empty url designates local EC ("local" means dll linked to executable, not EC running on local host)
            \param handler Functor with params: (ErrorCode, AbstractECConnectionPtr)
        */
        template<class TargetType, class HandlerType> int connect( const QUrl& addr, TargetType* target, HandlerType handler ) {
            return connectAsync( addr, std::static_pointer_cast<impl::ConnectHandler>(std::make_shared<impl::CustomConnectHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode connectSync( const QUrl& addr, AbstractECConnectionPtr* const connection ) {
            using namespace std::placeholders;
            return impl::doSyncCall<impl::ConnectHandler>( std::bind(std::mem_fn(&AbstractECConnectionFactory::connectAsync), this, addr, _1), connection );
        }

        virtual void registerRestHandlers( QnRestProcessorPool* const restProcessorPool ) = 0;
        virtual void registerTransactionListener( QnUniversalTcpListener* universalTcpListener ) = 0;
        virtual void setContext( const ResourceContext& resCtx ) = 0;

    protected:
        virtual int testConnectionAsync( const QUrl& addr, impl::TestConnectionHandlerPtr handler ) = 0;
        virtual int connectAsync( const QUrl& addr, impl::ConnectHandlerPtr handler ) = 0;
    };
}

Q_DECLARE_METATYPE(ec2::QnFullResourceData);

#endif  //EC_API_H
