#ifndef EC_API_H
#define EC_API_H

#include <algorithm>
#include <functional>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QList>
#include <QtCore/QUrl>

#include <utils/network/module_information.h>

#include <api/model/connection_info.h>
#include <api/model/email_attachment.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_user_attributes.h>
#include <core/resource/videowall_control_message.h>

#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>
#include <nx_ec/data/api_resource_data.h>
#include <nx_ec/data/api_email_data.h>
#include <nx_ec/data/api_server_alive_data.h>
#include <nx_ec/data/api_time_data.h>
#include <nx_ec/data/api_license_overflow_data.h>
#include <nx_ec/data/api_discovery_data.h>

#include "ec_api_fwd.h"

class QnRestProcessorPool;
class QnUniversalTcpListener;
struct QnModuleInformation;

//!Contains API classes for the new Server
/*!
    TODO describe all API classes
    \note All methods are thread-safe
*/
namespace ec2
{
    struct QnFullResourceData
    {
        QnResourceTypeList resTypes;
        QnResourceList resources;
        QnMediaServerUserAttributesList serverUserAttributesList;
        QnCameraUserAttributesList cameraUserAttributesList;
        QnBusinessEventRuleList bRules; // TODO: #Elric #EC2 rename
        QnCameraHistoryList cameraHistory;
        QnLicenseList licenses;
        ec2::ApiResourceParamWithRefDataList allProperties;
        ec2::ApiResourceStatusDataList resStatusList;
    };

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
        //void disabledChanged( const QnUuid& resourceId, bool disabled );
        void resourceChanged( const QnResourcePtr& resource );
        void resourceParamChanged( const ApiResourceParamWithRefData& param );
        void resourceRemoved( const QnUuid& resourceId );

    protected:
        virtual int getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) = 0;
        virtual int setResourceStatus( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler ) = 0;
        virtual int setResourceStatusLocal( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler ) = 0;
        //virtual int setResourceDisabled( const QnUuid& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler ) = 0;
        virtual int getKvPairs( const QnUuid &resourceId, impl::GetKvPairsHandlerPtr handler ) = 0;
        virtual int getStatusList( const QnUuid &resourceId, impl::GetStatusListHandlerPtr handler ) = 0;
        //virtual int save( const QnResourcePtr &resource, impl::SaveResourceHandlerPtr handler ) = 0;
        virtual int save(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) = 0;
        virtual int removeParams(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) = 0;
        virtual int remove( const QnUuid& resource, impl::SimpleHandlerPtr handler ) = 0;
        virtual int remove( const QVector<QnUuid>& resourceList, impl::SimpleHandlerPtr handler ) = 0;
    };


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
        template<class TargetType, class HandlerType> int getServers( const QnUuid& mediaServerId,  TargetType* target, HandlerType handler ) {
            return getServers(mediaServerId, std::static_pointer_cast<impl::GetServersHandler>(std::make_shared<impl::CustomGetServersHandler<TargetType, HandlerType>>(target, handler)) );
        }
        
        ErrorCode getServersSync( const QnUuid& mediaServerId, QnMediaServerResourceList* const serverList ) {
            return impl::doSyncCall<impl::GetServersHandler>( 
                [=](const impl::GetServersHandlerPtr &handler) {
                    return this->getServers(mediaServerId, handler);
                }, 
                serverList 
            );
        }

        /*!
            \param handler Functor with params: (ErrorCode, const QnMediaServerResourcePtr& servers)
        */
        template<class TargetType, class HandlerType> int save( const QnMediaServerResourcePtr& mserver, TargetType* target, HandlerType handler ) {
            return save( mserver, std::static_pointer_cast<impl::SaveServerHandler>(std::make_shared<impl::CustomSaveServerHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode saveSync( const QnMediaServerResourcePtr& serverRes, QnMediaServerResourcePtr* const server ) {
            return impl::doSyncCall<impl::SaveServerHandler>( 
                [=](const impl::SaveServerHandlerPtr &handler) {
                    return this->save(serverRes, handler);
                },
                server 
            );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int remove( const QnUuid& id, TargetType* target, HandlerType handler ) {
            return remove( id, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        template<class TargetType, class HandlerType> int remove( const QVector<QnUuid>& idList, TargetType* target, HandlerType handler ) {
            return remove( idList, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int saveUserAttributes( const QnMediaServerUserAttributesList& serverAttrs, TargetType* target, HandlerType handler ) {
            return saveUserAttributes( serverAttrs, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int saveStorages( const QnAbstractStorageResourceList& storages, TargetType* target, HandlerType handler ) {
            return saveStorages( storages, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode saveStoragesSync( const QnAbstractStorageResourceList& storages) {
            return impl::doSyncCall<impl::SimpleHandler>( 
                [=](const impl::SimpleHandlerPtr &handler) {
                    return this->saveStorages(storages, handler);
                }
            );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int removeStorages( const ApiIdDataList& storages, TargetType* target, HandlerType handler ) {
            return removeStorages( storages, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param mediaServerId if not NULL, returned list contains at most one element: the one, corresponding to \a mediaServerId. 
                If NULL, returned list contains data of all known servers
            \param handler Functor with params: (ErrorCode, const QnMediaServerUserAttributesList& serverUserAttributesList)
        */
        template<class TargetType, class HandlerType> int getUserAttributes( const QnUuid& mediaServerId, TargetType* target, HandlerType handler ) {
            return getUserAttributes( mediaServerId, std::static_pointer_cast<impl::GetServerUserAttributesHandler>(std::make_shared<impl::CustomGetServerUserAttributesHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param mediaServerId if not NULL, returned list contains at most one element: the one, corresponding to \a mediaServerId. 
                If NULL, returned list contains data of all known servers
        */
        ErrorCode getUserAttributesSync(const QnUuid& mediaServerId, QnMediaServerUserAttributesList* const serverAttrsList ) {
            int(AbstractMediaServerManager::*fn)(const QnUuid&, impl::GetServerUserAttributesHandlerPtr) = &AbstractMediaServerManager::getUserAttributes;
            return impl::doSyncCall<impl::GetServerUserAttributesHandler>( std::bind(fn, this, mediaServerId, std::placeholders::_1), serverAttrsList );
        }

        /*!
            \param mediaServerId if not NULL, returned list contains at most one element: the one, corresponding to \a mediaServerId. 
                If NULL, returned list contains data of all known servers
            \param handler Functor with params: (ErrorCode, const QnResourceList& storages)
        */
        template<class TargetType, class HandlerType> int getStorages( const QnUuid& mediaServerId, TargetType* target, HandlerType handler ) {
            return getStorages( mediaServerId, std::static_pointer_cast<impl::GetStoragesHandler>(std::make_shared<impl::CustomGetStoragesHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param mediaServerId if not NULL, returned list contains at most one element: the one, corresponding to \a mediaServerId. 
                If NULL, returned list contains data of all known servers
        */
        ErrorCode getStoragesSync(const QnUuid& mediaServerId, QnResourceList* const storages ) {
            int(AbstractMediaServerManager::*fn)(const QnUuid&, impl::GetStoragesHandlerPtr) = &AbstractMediaServerManager::getStorages;
            return impl::doSyncCall<impl::GetStoragesHandler>( std::bind(fn, this, mediaServerId, std::placeholders::_1), storages );
        }

    signals:
        void addedOrUpdated( QnMediaServerResourcePtr camera );
        void storageChanged( QnAbstractStorageResourcePtr camera );
        void removed( QnUuid id );
        void storageRemoved( QnUuid id );
        void userAttributesChanged( QnMediaServerUserAttributesPtr attributes );
        void userAttributesRemoved( QnUuid id );

    protected:
        virtual int getServers( const QnUuid& mediaServerId,  impl::GetServersHandlerPtr handler ) = 0;
        virtual int save( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) = 0;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) = 0;
        virtual int saveUserAttributes( const QnMediaServerUserAttributesList& serverAttrs, impl::SimpleHandlerPtr handler ) = 0;
        virtual int saveStorages( const QnAbstractStorageResourceList& storages, impl::SimpleHandlerPtr handler ) = 0;
        virtual int removeStorages( const ApiIdDataList& storages, impl::SimpleHandlerPtr handler ) = 0;
        virtual int getUserAttributes( const QnUuid& mediaServerId, impl::GetServerUserAttributesHandlerPtr handler ) = 0;
        virtual int getStorages( const QnUuid& mediaServerId, impl::GetStoragesHandlerPtr handler ) = 0;
    };


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
            int(AbstractCameraManager::*fn)(const QnVirtualCameraResourcePtr&, impl::AddCameraHandlerPtr) = &AbstractCameraManager::addCamera;
            return impl::doSyncCall<impl::AddCameraHandler>( std::bind(fn, this, camRes, std::placeholders::_1), cameras );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, TargetType* target, HandlerType handler ) {
            return addCameraHistoryItem( cameraHistoryItem, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        template<class TargetType, class HandlerType> int removeCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, TargetType* target, HandlerType handler ) {
            return removeCameraHistoryItem( cameraHistoryItem, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode addCameraHistoryItemSync( const QnCameraHistoryItem& historyItem) {
            int(AbstractCameraManager::*fn)(const QnCameraHistoryItem&, impl::SimpleHandlerPtr) = &AbstractCameraManager::addCameraHistoryItem;
            return impl::doSyncCall<impl::SimpleHandler>( std::bind(fn, this, historyItem, std::placeholders::_1));
        }
        ErrorCode removeCameraHistoryItemSync( const QnCameraHistoryItem& historyItem) {
            int(AbstractCameraManager::*fn)(const QnCameraHistoryItem&, impl::SimpleHandlerPtr) = &AbstractCameraManager::removeCameraHistoryItem;
            return impl::doSyncCall<impl::SimpleHandler>( std::bind(fn, this, historyItem, std::placeholders::_1));
        }

        /*!
            \param handler Functor with params: (ErrorCode, const QnVirtualCameraResourceList& cameras)
        */
        template<class TargetType, class HandlerType> int getCameras( const QnUuid& mediaServerId, TargetType* target, HandlerType handler ) {
            return getCameras( mediaServerId, std::static_pointer_cast<impl::GetCamerasHandler>(std::make_shared<impl::CustomGetCamerasHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getCamerasSync(const QnUuid& mServerId, QnVirtualCameraResourceList* const cameraList ) {
            int(AbstractCameraManager::*fn)(const QnUuid&, impl::GetCamerasHandlerPtr) = &AbstractCameraManager::getCameras;
            return impl::doSyncCall<impl::GetCamerasHandler>( std::bind(fn, this, mServerId, std::placeholders::_1), cameraList );
        }


        /*!
            \param handler Functor with params: (ErrorCode, const QnCameraHistoryList& cameras)
        */
        template<class TargetType, class HandlerType> int getCameraHistoryList( TargetType* target, HandlerType handler ) {
            return getCameraHistoryList( std::static_pointer_cast<impl::GetCamerasHistoryHandler>(std::make_shared<impl::CustomGetCamerasHistoryHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getCameraHistoryListSync(QnCameraHistoryList* const cameraHistoryList ) {
            int(AbstractCameraManager::*fn)(impl::GetCamerasHistoryHandlerPtr) = &AbstractCameraManager::getCameraHistoryList;
            return impl::doSyncCall<impl::GetCamerasHistoryHandler>( std::bind(fn, this,  std::placeholders::_1), cameraHistoryList );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save( const QnVirtualCameraResourceList& cameras, TargetType* target, HandlerType handler ) {
            return save( cameras, std::static_pointer_cast<impl::AddCameraHandler>(std::make_shared<impl::CustomAddCameraHandler<TargetType, HandlerType>>(target, handler)) );
        }

        //!Remove camera. Camera user attributes not removed by this method!
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int remove( const QnUuid& id, TargetType* target, HandlerType handler ) {
            return remove(id, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int saveUserAttributes( const QnCameraUserAttributesList& cameras, TargetType* target, HandlerType handler ) {
            return saveUserAttributes( cameras, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }
        
        ErrorCode saveUserAttributesSync( const QnCameraUserAttributesList& cameras) {
            int(AbstractCameraManager::*fn)(const QnCameraUserAttributesList&, impl::SimpleHandlerPtr) = &AbstractCameraManager::saveUserAttributes;
            return impl::doSyncCall<impl::SimpleHandler>( std::bind(fn, this, cameras, std::placeholders::_1));
        }

        /*!
            \param handler Functor with params: (ErrorCode, const QnCameraUserAttributesList& cameraUserAttributesList)
        */
        template<class TargetType, class HandlerType> int getUserAttributes( const QnUuid& cameraId, TargetType* target, HandlerType handler ) {
            return getUserAttributes( cameraId, std::static_pointer_cast<impl::GetCameraUserAttributesHandler>(std::make_shared<impl::CustomGetCameraUserAttributesHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getUserAttributesSync(const QnUuid& cameraId, QnCameraUserAttributesList* const cameraAttrsList ) {
            int(AbstractCameraManager::*fn)(const QnUuid&, impl::GetCameraUserAttributesHandlerPtr) = &AbstractCameraManager::getUserAttributes;
            return impl::doSyncCall<impl::GetCameraUserAttributesHandler>( std::bind(fn, this, cameraId, std::placeholders::_1), cameraAttrsList );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int addBookmarkTags( const QnCameraBookmarkTags &tags, TargetType* target, HandlerType handler ) {
            return addBookmarkTags(tags, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param handler Functor with params: (ErrorCode, const QnCameraBookmarkTags& usage)
        */
        template<class TargetType, class HandlerType> int getBookmarkTags( TargetType* target, HandlerType handler ) {
            return getBookmarkTags( std::static_pointer_cast<impl::GetCameraBookmarkTagsHandler>(std::make_shared<impl::CustomGetCameraBookmarkTagsHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int removeBookmarkTags( const QnCameraBookmarkTags &tags, TargetType* target, HandlerType handler ) {
            return removeBookmarkTags(tags, std::static_pointer_cast<impl::SimpleHandler>(std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void cameraAddedOrUpdated( QnVirtualCameraResourcePtr camera );
        void cameraHistoryChanged( QnCameraHistoryItemPtr cameraHistory );
        void cameraHistoryRemoved( QnCameraHistoryItemPtr cameraHistory );
        void cameraRemoved( QnUuid id );

        void userAttributesChanged( QnCameraUserAttributesPtr attributes );
        void userAttributesRemoved( QnUuid id );

        void cameraBookmarkTagsAdded(const QnCameraBookmarkTags &tags);
        void cameraBookmarkTagsRemoved(const QnCameraBookmarkTags &tags);

    protected:
        virtual int addCamera( const QnVirtualCameraResourcePtr&, impl::AddCameraHandlerPtr handler ) = 0;
        virtual int addCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler ) = 0;
        virtual int removeCameraHistoryItem( const QnCameraHistoryItem& cameraHistoryItem, impl::SimpleHandlerPtr handler ) = 0;
        virtual int getCameras( const QnUuid& mediaServerId, impl::GetCamerasHandlerPtr handler ) = 0;
        virtual int getCameraHistoryList( impl::GetCamerasHistoryHandlerPtr handler ) = 0;
        virtual int save( const QnVirtualCameraResourceList& cameras, impl::AddCameraHandlerPtr handler ) = 0;
        virtual int saveUserAttributes( const QnCameraUserAttributesList& cameras, impl::SimpleHandlerPtr handler ) = 0;
        virtual int getUserAttributes( const QnUuid& cameraId, impl::GetCameraUserAttributesHandlerPtr handler ) = 0;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) = 0;
        
        virtual int addBookmarkTags(const QnCameraBookmarkTags &tags, impl::SimpleHandlerPtr handler) = 0;
        virtual int getBookmarkTags(impl::GetCameraBookmarkTagsHandlerPtr handler) = 0;
        virtual int removeBookmarkTags(const QnCameraBookmarkTags &tags, impl::SimpleHandlerPtr handler) = 0;
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
        void businessRuleReset( const QnBusinessEventRuleList& rules );
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
        template<class TargetType, class HandlerType> int getUsers(const QnUuid& userId, TargetType* target, HandlerType handler ) {
            return getUsers(userId, std::static_pointer_cast<impl::GetUsersHandler>(
                std::make_shared<impl::CustomGetUsersHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getUsersSync(const QnUuid& userId, QnUserResourceList* const userList ) {
            int(AbstractUserManager::*fn)(const QnUuid&, impl::GetUsersHandlerPtr) = &AbstractUserManager::getUsers;
            return impl::doSyncCall<impl::GetUsersHandler>(std::bind(fn, this, userId, std::placeholders::_1), userList );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save( const QnUserResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return save( resource, std::static_pointer_cast<impl::AddUserHandler>(
                std::make_shared<impl::CustomAddUserHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode saveSync(const QnUserResourcePtr &userRes, QnUserResourceList * const users) {
            return impl::doSyncCall<impl::AddUserHandler>(
                [=](const impl::AddUserHandlerPtr &handler) {
                    return this->save(userRes, handler);
                },
                users
            );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int remove( const QnUuid& id, TargetType* target, HandlerType handler ) {
            return remove( id, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void addedOrUpdated( QnUserResourcePtr camera );
        void removed( QnUuid id );

    private:
        virtual int getUsers(const QnUuid& userId, impl::GetUsersHandlerPtr handler ) = 0;
        virtual int save( const QnUserResourcePtr& resource, impl::AddUserHandlerPtr handler ) = 0;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) = 0;
    };


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
        template<class TargetType, class HandlerType> int remove( const QnUuid& resource, TargetType* target, HandlerType handler ) {
            return remove( resource, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void addedOrUpdated( QnLayoutResourcePtr camera );
        void removed( QnUuid id );

    protected:
        virtual int getLayouts( impl::GetLayoutsHandlerPtr handler ) = 0;
        virtual int save( const QnLayoutResourceList& resources, impl::SimpleHandlerPtr handler ) = 0;
        virtual int remove( const QnUuid& resource, impl::SimpleHandlerPtr handler ) = 0;
    };
    

    /*!
        \note All methods are asynchronous if other not specified
    */
    class AbstractVideowallManager
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractVideowallManager() {}

        /*!
            \param handler Functor with params: (ErrorCode, const QnUserResourceList&)
        */
        template<class TargetType, class HandlerType> int getVideowalls( TargetType* target, HandlerType handler ) {
            return getVideowalls( std::static_pointer_cast<impl::GetVideowallsHandler>(
                std::make_shared<impl::CustomGetVideowallsHandler<TargetType, HandlerType>>(target, handler)) );
        }

        ErrorCode getVideowallsSync(QnVideoWallResourceList* const videowallList ) {
            int(AbstractVideowallManager::*fn)(impl::GetVideowallsHandlerPtr) = &AbstractVideowallManager::getVideowalls;
            return impl::doSyncCall<impl::GetVideowallsHandler>( std::bind(fn, this, std::placeholders::_1), videowallList );
        }


        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int save( const QnVideoWallResourcePtr& resource, TargetType* target, HandlerType handler ) {
            return save( resource, std::static_pointer_cast<impl::AddVideowallHandler>(
                std::make_shared<impl::CustomAddVideowallHandler<TargetType, HandlerType>>(target, handler)) );
        }
        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int remove( const QnUuid& id, TargetType* target, HandlerType handler ) {
            return remove( id, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

        /*!
            \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType> int sendControlMessage(const QnVideoWallControlMessage& message, TargetType* target, HandlerType handler ) {
            return sendControlMessage(message, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)) );
        }

    signals:
        void addedOrUpdated(const QnVideoWallResourcePtr &videowall);
        void removed(const QnUuid &id);
        void controlMessage(const QnVideoWallControlMessage &message);

    protected:
        virtual int getVideowalls( impl::GetVideowallsHandlerPtr handler ) = 0;
        virtual int save( const QnVideoWallResourcePtr& resource, impl::AddVideowallHandlerPtr handler ) = 0;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) = 0;

        virtual int sendControlMessage(const QnVideoWallControlMessage& message, impl::SimpleHandlerPtr handler) = 0;
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

    signals:
        void peerDiscoveryRequested(const QUrl &url);
        void discoveryInformationChanged(const ApiDiscoveryData &data, bool addInformation);

    protected:
        virtual int discoverPeer(const QUrl &url, impl::SimpleHandlerPtr handler) = 0;
        virtual int addDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) = 0;
        virtual int removeDiscoveryInformation(const QnUuid &id, const QUrl &url, bool ignore, impl::SimpleHandlerPtr handler) = 0;
        virtual int getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler) = 0;
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
        template<class TargetType, class HandlerType> int sendModuleInformation(const QnModuleInformation &moduleInformation, bool isAlive, TargetType *target, HandlerType handler) {
            return sendModuleInformation(moduleInformation, isAlive, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType> int sendModuleInformationList(const QList<QnModuleInformation> &moduleInformationList, TargetType *target, HandlerType handler) {
            return sendModuleInformation(moduleInformationList, std::static_pointer_cast<impl::SimpleHandler>(
                 std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        template<class TargetType, class HandlerType> int changeSystemName(const QString &systemName, TargetType *target, HandlerType handler) {
            return changeSystemName(systemName, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode changeSystemNameSync(const QString &systemName) {
            return impl::doSyncCall<impl::SimpleHandler>(
                [=](const impl::SimpleHandlerPtr &handler) {
                    return this->changeSystemName(systemName, handler);
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
        void moduleChanged(const QnModuleInformation &moduleInformation, bool isAlive);
        void systemNameChangeRequested(const QString &systemName);

    protected:
        virtual int sendModuleInformation(const QnModuleInformation &moduleInformation, bool isAlive, impl::SimpleHandlerPtr handler) = 0;
        virtual int sendModuleInformationList(const QList<QnModuleInformation> &moduleInformationList, impl::SimpleHandlerPtr handler) = 0;
        virtual int changeSystemName(const QString &systemName, impl::SimpleHandlerPtr handler) = 0;
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
        virtual void addRemotePeer(const QUrl& url) = 0;
        virtual void deleteRemotePeer(const QUrl& url) = 0;
        virtual void sendRuntimeData(const ec2::ApiRuntimeData &data) = 0;

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
        void initNotification(QnFullResourceData fullData);
        void runtimeInfoChanged(const ec2::ApiRuntimeData& runtimeInfo);

        void remotePeerFound(ApiPeerAliveData data);
        void remotePeerLost(ApiPeerAliveData data);
        void remotePeerUnauthorized(const QnUuid& id);

        void settingsChanged(ec2::ApiResourceParamDataList settings);

        void databaseDumped();

    protected:
        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) = 0;
        virtual int restoreDatabaseAsync( const ec2::ApiDatabaseDumpData& data, impl::SimpleHandlerPtr handler ) = 0;
    };  


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
        template<class TargetType, class HandlerType> int connect( const QUrl& addr, TargetType* target, HandlerType handler ) {
            return connectAsync( addr, std::static_pointer_cast<impl::ConnectHandler>(std::make_shared<impl::CustomConnectHandler<TargetType, HandlerType>>(target, handler)) );
        }
        ErrorCode connectSync( const QUrl& addr, AbstractECConnectionPtr* const connection ) {
            return impl::doSyncCall<impl::ConnectHandler>( std::bind(std::mem_fn(&AbstractECConnectionFactory::connectAsync), this, addr, std::placeholders::_1), connection );
        }

        virtual void registerRestHandlers( QnRestProcessorPool* const restProcessorPool ) = 0;
        virtual void registerTransactionListener( QnUniversalTcpListener* universalTcpListener ) = 0;
        virtual void setContext( const ResourceContext& resCtx ) = 0;

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
        virtual int connectAsync( const QUrl& addr, impl::ConnectHandlerPtr handler ) = 0;

    private:
        bool m_compatibilityMode;
    };
}

Q_DECLARE_METATYPE(ec2::QnFullResourceData);
Q_DECLARE_METATYPE(ec2::QnPeerTimeInfoList);

#endif  //EC_API_H
