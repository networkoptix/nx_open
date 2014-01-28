
#ifndef EC_API_H
#define EC_API_H

#include <memory>

#include <QtCore/QObject>

#include "api/model/email_attachment.h"
#include "impl/ec_api_impl.h"
#include "impl/sync_handler.h"


//!Contains API classes for the new enterprise controller
/*!
    TODO describe all API classes
    \note All methods are thread-safe
*/
namespace ec2
{
    typedef int ReqID;
    const ReqID INVALID_REQ_ID = -1;

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
        template<class TargetType, class HandlerType> ReqID getResourceTypes( TargetType* target, HandlerType handler ) {
            return getResourceTypes( std::static_pointer_cast<impl::GetResourceTypesHandler>(std::make_shared<impl::CustomGetResourceTypesHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QnResourceList&)
        */
        template<class TargetType, class HandlerType> ReqID getResources( TargetType* target, HandlerType handler ) {
            return getResources( std::static_pointer_cast<impl::GetResourcesHandler>(std::make_shared<impl::CustomGetResourcesHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QnResourcePtr&)
        */
        template<class TargetType, class HandlerType> ReqID getResource( const QnId& id, TargetType* target, HandlerType handler ) {
            return getResource( std::static_pointer_cast<impl::GetResourceHandler>(std::make_shared<impl::CustomGetResourceHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID setResourceStatus( const QnId& resourceId, QnResource::Status status, TargetType* target, HandlerType handler ) {
            return setResourceStatus( std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QnKvPairListsById&)
        */
        template<class TargetType, class HandlerType> ReqID getKvPairs( const QnResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return getKvPairs( resource, std::static_pointer_cast<impl::GetKvPairsHandler>(std::make_shared<impl::CustomGetKvPairsHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID setResourceDisabled( const QnId& resourceId, bool disabled, TargetType* target, HandlerType handler ) {
            return setResourceDisabled( resourceId, disabled, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID save( const QnResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return save( std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID save( int resourceId, const QnKvPairList& kvPairs, TargetType* target, HandlerType handler ) {
            return save( std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID remove( const QnResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return remove( std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }


    protected:
        virtual ReqID getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) = 0;
        virtual ReqID getResources( impl::GetResourcesHandlerPtr handler ) = 0;
        virtual ReqID getResource( const QnId& id, impl::GetResourceHandlerPtr handler ) = 0;
        virtual ReqID setResourceStatus( const QnId& resourceId, QnResource::Status status, impl::SimpleHandlerPtr handler ) = 0;

        virtual ReqID getKvPairs( const QnResourcePtr &resource, impl::GetKvPairsHandlerPtr handler ) = 0;
        virtual ReqID setResourceDisabled( const QnId& resourceId, bool disabled, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID save( const QnResourcePtr &resource, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID save( int resourceId, const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID remove( const QnResourcePtr& resource, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractResourceManager> AbstractResourceManagerPtr;


    class AbstractMediaServerManager
    {
    public:
        virtual ~AbstractMediaServerManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnMediaServerResourceList& servers)
        */
        template<class TargetType, class HandlerType> ReqID getServers( TargetType* target, HandlerType handler ) {
            return getServers( std::static_pointer_cast<impl::GetServersHandler>(std::make_shared<impl::CustomGetServersHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID save( const QnMediaServerResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return save( std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QnMediaServerResourceList& servers, const QByteArray& authKey)
        */
        template<class TargetType, class HandlerType> ReqID saveServer( const QnMediaServerResourcePtr& mserver, TargetType* target, HandlerType handler ) {
            return saveServer( mserver, std::static_pointer_cast<impl::SaveServerHandler>(std::make_shared<impl::CustomSaveServerHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID remove( const QnMediaServerResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return remove( resource, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    protected:
        virtual ReqID getServers( impl::GetServersHandlerPtr handler ) = 0;
        virtual ReqID save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID saveServer( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) = 0;
        virtual ReqID remove( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler ) = 0;
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
            \param handler Functor with params: (ErrorCode, const QnVirtualCameraResourceListPtr& cameras)
        */
        template<class TargetType, class HandlerType> ReqID addCamera( const QnVirtualCameraResourcePtr& camRes, TargetType* target, HandlerType handler, Qt::ConnectionType connectionType = Qt::AutoConnection ) {
            return addCamera( camRes, std::static_pointer_cast<impl::AddCameraHandler>(std::make_shared<impl::CustomAddCameraHandler<TargetType, HandlerType>>(target, handler, connectionType)) );
        }
        ErrorCode addCameraSync( const QnVirtualCameraResourcePtr& camRes, QnVirtualCameraResourceListPtr* const cameras ) {
            SyncHandler syncHandler;
            addCamera( camRes, &syncHandler, &SyncHandler::done, Qt::DirectConnection );
            syncHandler.wait();
            return syncHandler.errorCode();
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem ) {
            return addCameraHistoryItem( std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QnVirtualCameraResourceList& cameras)
        */
        template<class TargetType, class HandlerType> ReqID getCameras( QnId mediaServerId, TargetType* target, HandlerType handler ) {
            return getCameras( std::static_pointer_cast<impl::GetCamerasHandler>(std::make_shared<impl::CustomGetCamerasHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QnCameraHistoryList& cameras)
        */
        template<class TargetType, class HandlerType> ReqID getCameraHistoryList( TargetType* target, HandlerType handler ) {
            return getCameraHistoryList( std::static_pointer_cast<impl::GetCamerasHistoryHandler>(std::make_shared<impl::CustomGetCamerasHistoryHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID save( const QnVirtualCameraResourceList& cameras, TargetType* target, HandlerType handler ) {
            return save( std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID remove( const QnVirtualCameraResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return remove( std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void cameraHistoryChanged(QnCameraHistoryItemPtr cameraHistory);

    protected:
        virtual ReqID addCamera( const QnVirtualCameraResourcePtr&, impl::AddCameraHandlerPtr handler ) = 0;
        virtual ReqID addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID getCameras( QnId mediaServerId, impl::GetCamerasHandlerPtr handler ) = 0;
        virtual ReqID getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler ) = 0;
        virtual ReqID save( const QnVirtualCameraResourceList& cameras, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID remove( const QnVirtualCameraResourcePtr& resource, impl::SimpleHandlerPtr handler ) = 0;
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
        template<class TargetType, class HandlerType> ReqID getLicenses( TargetType* target, HandlerType handler ) {
            return getLicenses( std::static_pointer_cast<impl::GetLicensesHandler>(std::make_shared<impl::CustomGetLicensesHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID addLicenses( const QList<QnLicensePtr>& licenses, TargetType* target, HandlerType handler ) {
            return addLicensesAsync( licenses, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void licenseChanged(QnLicensePtr license);

    protected:
        virtual ReqID getLicenses( impl::GetLicensesHandlerPtr handler ) = 0;
        virtual ReqID addLicensesAsync( const QList<QnLicensePtr>& licenses, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractLicenseManager> AbstractLicenseManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractBusinessEventManager: public QObject
    {
    public:
        virtual ~AbstractBusinessEventManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnBusinessEventRuleList&)
        */
        template<class TargetType, class HandlerType> ReqID getBusinessRules( TargetType* target, HandlerType handler ) {
            return getBusinessRules( std::static_pointer_cast<impl::GetBusinessRules>(std::make_shared<impl::CustomGetBusinessRules<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID addBusinessRule( const QnBusinessEventRulePtr& businessRule, TargetType* target, HandlerType handler ) {
            return addBusinessRule( rule, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        //!Test if email settings are valid
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID testEmailSettings( const QnKvPairList& settings, TargetType* target, HandlerType handler ) {
            return testEmailSettings( rule, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param to Destination address list
            \param timeout TODO
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID sendEmail(
            const QStringList& to,
            const QString& subject,
            const QString& message,
            int timeout,
            const QnEmailAttachmentList& attachments,
            TargetType* target, HandlerType handler )
        {
            return sendEmail( rule, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID save( const QnBusinessEventRulePtr& rule, TargetType* target, HandlerType handler ) {
            return save( rule, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID deleteRule( int ruleId, TargetType* target, HandlerType handler ) {
            return deleteRule( rule, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, TargetType* target, HandlerType handler ) {
            return broadcastBusinessAction( rule, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID resetBusinessRules( TargetType* target, HandlerType handler ) {
            return resetBusinessRules( rule, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    private:
        virtual ReqID getBusinessRules( impl::GetBusinessRulesHandlerPtr handler ) = 0;
        virtual ReqID addBusinessRule( const QnBusinessEventRulePtr& businessRule, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID testEmailSettings( const QnKvPairList& settings, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID sendEmail(
            const QStringList& to,
            const QString& subject,
            const QString& message,
            int timeout,
            const QnEmailAttachmentList& attachments,
            impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID save( const QnBusinessEventRulePtr& rule, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID deleteRule( int ruleId, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID resetBusinessRules( impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractBusinessEventManager> AbstractBusinessEventManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractUserManager
    {
    public:
        virtual ~AbstractUserManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnUserResourceList&)
        */
        template<class TargetType, class HandlerType> ReqID getUsers( QnUserResourceList& users, TargetType* target, HandlerType handler ) {
            return getUsers( getUsers, std::static_pointer_cast<impl::GetUsersHandler>(std::make_shared<impl::CustomGetUsersHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID save( const QnUserResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return save( resource, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID remove( const QnUserResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return remove( resource, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    private:
        virtual ReqID getUsers( impl::GetUsersHandlerPtr handler ) = 0;
        virtual ReqID save( const QnUserResourcePtr& resource, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID remove( const QnUserResourcePtr& resource, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractUserManager> AbstractUserManagerPtr;

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractLayoutManager
    {
    public:
        virtual ~AbstractLayoutManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnLayoutResourceList&)
        */
        template<class TargetType, class HandlerType> ReqID getLayouts( TargetType* target, HandlerType handler ) {
            return getLayouts( std::static_pointer_cast<impl::GetLayoutsHandler>(std::make_shared<impl::CustomGetLayoutsHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID save( const QnLayoutResourceList& resources, TargetType* target, HandlerType handler ) {
            return save( resources, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID remove( const QnLayoutResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return remove( resource, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    protected:
        virtual ReqID getLayouts( impl::GetLayoutsHandlerPtr handler ) = 0;
        virtual ReqID save( const QnLayoutResourceList& resources, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID remove( const QnLayoutResourcePtr& resource, impl::SimpleHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractLayoutManager> AbstractLayoutManagerPtr;

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
        template<class TargetType, class HandlerType> ReqID getStoredFile( const QString& filename, TargetType* target, HandlerType handler ) {
            return getStoredFile( filename, std::static_pointer_cast<impl::GetStoredFileHandler>(std::make_shared<impl::impl::CustomGetStoredFileHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID addStoredFile( const QString& filename, const QByteArray& data, TargetType* target, HandlerType handler ) {
            return addStoredFile( filename, data, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID deleteStoredFile( const QString& filename, TargetType* target, HandlerType handler ) {
            return deleteStoredFile( filename, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QStringList& filenames)
        */
        template<class TargetType, class HandlerType> ReqID listDirectory( const QString& folderName, TargetType* target, HandlerType handler ) {
            return listDirectory( folderName, std::static_pointer_cast<impl::ListDirectoryHandler>(std::make_shared<impl::impl::CustomListDirectoryHandler<TargetType, HandlerType>>(target, handler)) );
        }

    protected:
        virtual ReqID getStoredFile( const QString& filename, impl::GetStoredFileHandlerPtr handler ) = 0;
        virtual ReqID addStoredFile( const QString& filename, const QByteArray& data, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID deleteStoredFile( const QString& filename, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID listDirectory( const QString& folderName, impl::ListDirectoryHandlerPtr handler ) = 0;
    };
    typedef std::shared_ptr<AbstractStoredFileManager> AbstractStoredFileManagerPtr;

    enum class PanicMode
    {
        on,
        off
    };

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractECConnection
    {
    public:
        virtual ~AbstractECConnection() {}

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
        template<class TargetType, class HandlerType> ReqID setPanicMode( PanicMode value, TargetType* target, HandlerType handler ) {
            return setPanicMode( value, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param handler Functor with params: (ErrorCode, qint64)
        */
        template<class TargetType, class HandlerType> ReqID getCurrentTime( TargetType* target, HandlerType handler ) {
            return getCurrentTime( std::static_pointer_cast<impl::CurrentTimeHandler>(std::make_shared<impl::impl::CustomCurrentTimeHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, QByteArray dbFile)
        */
        template<class TargetType, class HandlerType> ReqID dumpDatabaseAsync( TargetType* target, HandlerType handler ) {
            return dumpDatabaseAsync( std::static_pointer_cast<impl::DumpDatabaseHandler>(std::make_shared<impl::impl::CustomDumpDatabaseHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID restoreDatabaseAsync( const QByteArray& dbFile, TargetType* target, HandlerType handler ) {
            return restoreDatabaseAsync( dbFile, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, const QnKvPairList&)
            \return Request id
        */
        template<class TargetType, class HandlerType> ReqID getSettingsAsync( TargetType* target, HandlerType handler ) {
            return getSettingsAsync( std::static_pointer_cast<impl::GetSettingsHandler>(std::make_shared<impl::impl::CustomGetSettingsHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> ReqID saveSettingsAsync( const QnKvPairList& kvPairs ) {
            return saveSettingsAsync( kvPairs, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        //!Cancel running async request
        /*!
            \warning Request handler may still be called after return of this method, since request could already have been completed and resulted posted to handler
        */
        //virtual void cancelRequest( ReqID requestID ) = 0;

    protected:
        virtual ReqID setPanicMode( PanicMode value, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID getCurrentTime( impl::CurrentTimeHandlerPtr handler ) = 0;
        virtual ReqID dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) = 0;
        virtual ReqID restoreDatabaseAsync( const QByteArray& dbFile, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID getSettingsAsync( impl::GetSettingsHandlerPtr handler ) = 0;
        virtual ReqID saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) = 0;
    };  

    typedef std::shared_ptr<AbstractECConnection> AbstractECConnectionPtr;

    class ECAddress
    {
        //TODO/IMPL
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
        template<class TargetType, class HandlerType> ReqID testConnection( const ECAddress& addr, TargetType* target, HandlerType handler ) {
            return testConnectionAsync( addr, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode, AbstractECConnectionPtr)
        */
        template<class TargetType, class HandlerType> ReqID connect( const ECAddress& addr, TargetType* target, HandlerType handler, Qt::ConnectionType connectionType = Qt::AutoConnection ) {
            return connectAsync( addr, std::static_pointer_cast<impl::ConnectHandler>(std::make_shared<impl::CustomConnectHandler<TargetType, HandlerType>>(target, handler, connectionType)) );
        }
        ErrorCode connectSync( const ECAddress& addr, AbstractECConnectionPtr* const connection ) {
            SyncHandler syncHandler;
            connect( addr, &syncHandler, &SyncHandler::done, Qt::DirectConnection );
            syncHandler.wait();
            return syncHandler.errorCode();
        }

    protected:
        virtual ReqID testConnectionAsync( const ECAddress& addr, impl::SimpleHandlerPtr handler ) = 0;
        virtual ReqID connectAsync( const ECAddress& addr, impl::ConnectHandlerPtr handler ) = 0;
    };
}

#endif  //EC_API_H
