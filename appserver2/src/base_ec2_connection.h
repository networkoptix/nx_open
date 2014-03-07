/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef BASE_EC2_CONNECTION_H
#define BASE_EC2_CONNECTION_H

#include <memory>

#include "nx_ec/ec_api.h"
#include "core/resource_management/resource_pool.h"
#include "nx_ec/data/mserver_data.h"
#include "transaction/transaction.h"
#include "business_event_manager.h"
#include "camera_manager.h"
#include "managers/layout_manager.h"
#include "managers/license_manager.h"
#include "managers/stored_file_manager.h"
#include "media_server_manager.h"
#include "resource_manager.h"
#include "user_manager.h"

#include "nx_ec/data/ec2_full_data.h"


namespace ec2
{
    template<class QueryProcessorType>
    class BaseEc2Connection
    :
        public AbstractECConnection
    {
    public:
        BaseEc2Connection(
            QueryProcessorType* queryProcessor,
            const ResourceContext& resCtx );

        virtual AbstractResourceManagerPtr getResourceManager() override;
        virtual AbstractMediaServerManagerPtr getMediaServerManager() override;
        virtual AbstractCameraManagerPtr getCameraManager() override;
        virtual AbstractLicenseManagerPtr getLicenseManager() override;
        virtual AbstractBusinessEventManagerPtr getBusinessEventManager() override;
        virtual AbstractUserManagerPtr getUserManager() override;
        virtual AbstractLayoutManagerPtr getLayoutManager() override;
        virtual AbstractStoredFileManagerPtr getStoredFileManager() override;

        virtual int setPanicMode( Qn::PanicMode value, impl::SimpleHandlerPtr handler ) override;
        virtual int getCurrentTime( impl::CurrentTimeHandlerPtr handler ) override;
        virtual int dumpDatabaseAsync( impl::DumpDatabaseHandlerPtr handler ) override;
        virtual int restoreDatabaseAsync( const QByteArray& dbFile, impl::SimpleHandlerPtr handler ) override;
        virtual int getSettingsAsync( impl::GetSettingsHandlerPtr handler ) override;
        virtual int saveSettingsAsync( const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) override;

        virtual void addRemotePeer(const QUrl& url, bool isClient) override;
        virtual void deleteRemotePeer(const QUrl& url) override;
    public:

        template<class T> bool processIncomingTransaction( const QnTransaction<T>& tran, const QByteArray& serializedTran) {
            if (!m_queryProcessor->processIncomingTransaction(tran, serializedTran))
                return false;
            triggerNotification(tran);
            return true;
        }

        void triggerNotification( const QnTransaction<ApiLicenseList>& tran ) {
            m_licenseManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiResetBusinessRuleData>& tran ) {
            m_businessEventManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiCameraData>& tran ) {
            m_cameraManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiCameraDataList>& tran ) {
            m_cameraManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiBusinessActionData>& tran ) {
            m_businessEventManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran ) {
            switch( tran.command )
            {
            case ApiCommand::removeResource:
                return m_resourceManager->triggerNotification( tran );
            case ApiCommand::removeCamera:
                return m_cameraManager->triggerNotification( tran );
            case ApiCommand::removeMediaServer:
                return m_mediaServerManager->triggerNotification( tran );
            case ApiCommand::removeUser:
                return m_userManager->triggerNotification( tran );
            case ApiCommand::removeBusinessRule:
                return m_businessEventManager->triggerNotification( tran );
            case ApiCommand::removeLayout:
                return m_layoutManager->triggerNotification( tran );
            default:
                assert( false );
            }
        }

        void triggerNotification( const QnTransaction<ApiMediaServerData>& tran ) {
            m_mediaServerManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiResourceData>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiSetResourceStatusData>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiSetResourceDisabledData>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiResourceParams>& tran ) {
            m_resourceManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiCameraServerItemData>& tran ) {
            return m_cameraManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiUserData>& tran ) {
            return m_userManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiBusinessRuleData>& tran ) {
            return m_businessEventManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiLayoutData>& tran ) {
            return m_layoutManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran ) {
            return m_layoutManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiStoredFileData>& tran ) {
            return m_storedFileManager->triggerNotification( tran );
        }

        void triggerNotification( const QnTransaction<ApiFullData>& tran ) {
            QnFullResourceData fullResData;
            tran.params.toResourceList( fullResData, m_resCtx );
            emit initNotification(fullResData);
        }

        void triggerNotification( const QnTransaction<ApiPanicModeData>& /*tran*/ ) {
            //TODO/IMPL
        }

        void triggerNotification( const QnTransaction<QString>& tran ) {
            if( tran.command == ApiCommand::removeStoredFile )
                m_storedFileManager->triggerNotification( tran );
        }

        QueryProcessorType* queryProcessor() const { return m_queryProcessor; }
    protected:
        QueryProcessorType* m_queryProcessor;
        ResourceContext m_resCtx;
        std::shared_ptr<QnLicenseManager<QueryProcessorType>> m_licenseManager;
        std::shared_ptr<QnResourceManager<QueryProcessorType>> m_resourceManager;
        std::shared_ptr<QnMediaServerManager<QueryProcessorType>> m_mediaServerManager;
        std::shared_ptr<QnCameraManager<QueryProcessorType>> m_cameraManager;
        std::shared_ptr<QnUserManager<QueryProcessorType>> m_userManager;
        std::shared_ptr<QnBusinessEventManager<QueryProcessorType>> m_businessEventManager;
        std::shared_ptr<QnLayoutManager<QueryProcessorType>> m_layoutManager;
        std::shared_ptr<QnStoredFileManager<QueryProcessorType>> m_storedFileManager;

    private:
        QnTransaction<ApiPanicModeData> prepareTransaction( ApiCommand::Value cmd, const Qn::PanicMode& mode);
    };
}

#endif  //BASE_EC2_CONNECTION_H
