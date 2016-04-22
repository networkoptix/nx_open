#include <iostream>
#include "transaction_descriptor.h"
#include "transaction_message_bus.h"
#include "nx_ec/data/api_tran_state_data.h"

namespace ec2
{
namespace detail
{
    template<typename T, typename F>
    ErrorCode saveTransactionImpl(const QnTransaction<T>& tran, ec2::QnTransactionLog *tlog, F f)
    {
        QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
        return tlog->saveToDB(tran, f(tran.params), serializedTran);
    }

    template<typename T, typename F>
    ErrorCode saveSerializedTransactionImpl(const QnTransaction<T>& tran, const QByteArray& serializedTran, QnTransactionLog *tlog, F f)
    {
        return tlog->saveToDB(tran, f(tran.params), serializedTran);
    }

    struct InvalidGetHashHelper
    {
        template<typename Param>
        QnUuid operator ()(const Param &)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!");
            return QnUuid();
        }
    };

    template<typename GetHash>
    struct DefaultSaveTransactionHelper
    {
        template<typename Param>
        ErrorCode operator ()(const QnTransaction<Param> &tran, QnTransactionLog *tlog)
        {
            return saveTransactionImpl(tran, tlog, m_getHash);
        }

        DefaultSaveTransactionHelper(GetHash getHash) : m_getHash(getHash) {}
    private:
        GetHash m_getHash;
    };

    template<typename GetHash>
    constexpr auto createDefaultSaveTransactionHelper(GetHash getHash)
    {
        return DefaultSaveTransactionHelper<GetHash>(getHash);
    }

    template<typename GetHash>
    struct DefaultSaveSerializedTransactionHelper
    {
        template<typename Param>
        ErrorCode operator ()(const QnTransaction<Param> &tran, const QByteArray &serializedTran, QnTransactionLog *tlog)
        {
            return saveSerializedTransactionImpl(tran, serializedTran, tlog, m_getHash);
        }

        DefaultSaveSerializedTransactionHelper(GetHash getHash) : m_getHash(getHash) {}
    private:
        GetHash m_getHash;
    };

    template<typename GetHash>
    constexpr auto createDefaultSaveSerializedTransactionHelper(GetHash getHash)
    {
        return DefaultSaveSerializedTransactionHelper<GetHash>(getHash);
    }

    struct InvalidSaveSerializedTransactionHelper
    {
        template<typename Param>
        ErrorCode operator ()(const QnTransaction<Param> &, const QByteArray &, QnTransactionLog *)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
    };

    struct InvalidTriggerNotificationHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &, const NotificationParams &)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
    };

    struct CreateHashByIdHelper
    {
        template<typename Param>
        QnUuid operator ()(const Param &param) { return param.id; }
    };

    struct CreateHashByUserIdHelper
    {
        template<typename Param>
        QnUuid operator ()(const Param &param) { return param.userId; }
    };

    struct CreateHashByIdRfc4122Helper
    {
        template<typename Param>
        QnUuid operator ()(const Param &param) { return QnTransactionLog::makeHash(param.id.toRfc4122()); }
    };

    struct CreateHashForResourceParamWithRefDataHelper
    {
        QnUuid operator ()(const ApiResourceParamWithRefData &param)
        {
            QCryptographicHash hash(QCryptographicHash::Md5);
            hash.addData("res_params");
            hash.addData(param.resourceId.toRfc4122());
            hash.addData(param.name.toUtf8());
            return QnUuid::fromRfc4122(hash.result());
        }
    };

    void apiIdDataTriggerNotificationHelper(const QnTransaction<ApiIdData> &tran, const NotificationParams &notificationParams)
    {
        switch( tran.command )
        {
        case ApiCommand::removeResource:
            return notificationParams.resourceNotificationManager->triggerNotification( tran );
        case ApiCommand::removeCamera:
            return notificationParams.cameraNotificationManager->triggerNotification( tran );
        case ApiCommand::removeMediaServer:
        case ApiCommand::removeStorage:
            return notificationParams.mediaServerNotificationManager->triggerNotification( tran );
        case ApiCommand::removeUser:
            return notificationParams.userNotificationManager->triggerNotification( tran );
        case ApiCommand::removeBusinessRule:
            return notificationParams.businessEventNotificationManager->triggerNotification( tran );
        case ApiCommand::removeLayout:
            return notificationParams.layoutNotificationManager->triggerNotification( tran );
        case ApiCommand::removeVideowall:
            return notificationParams.videowallNotificationManager->triggerNotification( tran );
        case ApiCommand::removeWebPage:
            return notificationParams.webPageNotificationManager->triggerNotification( tran );
        case ApiCommand::forcePrimaryTimeServer:
            //#ak no notification needed
            break;
        default:
            NX_ASSERT( false );
        }
    }

    QnUuid createHashForServerFootageDataHelper(const ApiServerFootageData &params)
    {
        return QnTransactionLog::makeHash(params.serverGuid.toRfc4122(), "history");
    }

    QnUuid createHashForApiCameraAttributesDataHelper(const ApiCameraAttributesData &params)
    {
        return QnTransactionLog::makeHash(params.cameraID.toRfc4122(), "camera_attributes");
    }

    QnUuid createHashForApiMediaServerUserAttributesDataHelper(const ApiMediaServerUserAttributesData &params)
    {
        return QnTransactionLog::makeHash(params.serverID.toRfc4122(), "server_attributes");
    }

    QnUuid createHashForApiStoredFileDataHelper(const ApiStoredFileData &params)
    {
        return QnTransactionLog::makeHash(params.path.toUtf8());
    }

    QnUuid createHashForApiDiscoveryDataHelper(const ApiDiscoveryData& params)
    {
         return QnTransactionLog::makeHash("discovery_data", params);
    }

    struct CameraNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.cameraNotificationManager->triggerNotification(tran);
        }
    };

    struct ResourceNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.resourceNotificationManager->triggerNotification(tran);
        }
    };

    struct MediaServerNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.mediaServerNotificationManager->triggerNotification(tran);
        }
    };

    struct UserNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.userNotificationManager->triggerNotification(tran);
        }
    };

    struct LayoutNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.layoutNotificationManager->triggerNotification(tran);
        }
    };

    struct VideowallNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.videowallNotificationManager->triggerNotification(tran);
        }
    };

    struct BusinessEventNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.businessEventNotificationManager->triggerNotification(tran);
        }
    };

    struct StoredFileNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.storedFileNotificationManager->triggerNotification(tran);
        }
    };

    struct LicenseNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.licenseNotificationManager->triggerNotification(tran);
        }
    };

    struct UpdateNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.updatesNotificationManager->triggerNotification(tran);
        }
    };

    struct DiscoveryNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.discoveryNotificationManager->triggerNotification(tran);
        }
    };

    struct WebPageNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, const NotificationParams &notificationParams)
        {
            notificationParams.webPageNotificationManager->triggerNotification(tran);
        }
    };

    struct EmptyNotificationHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &, const NotificationParams &) {}
    };

    void apiIdDataListTriggerNotificationHelper(const QnTransaction<ApiIdDataList> &tran, const NotificationParams &notificationParams)
    {
         switch( tran.command )
         {
         case ApiCommand::removeStorages:
             return notificationParams.mediaServerNotificationManager->triggerNotification( tran );
         case ApiCommand::removeResources:
             return notificationParams.resourceNotificationManager->triggerNotification( tran );
         default:
             NX_ASSERT( false );
        }
    }

#define TRANSACTION_DESCRIPTOR(Key, ParamType, isPersistent, isSystem, getHashFunc, triggerNotificationFunc) \
    TransactionDescriptor<Key, ParamType>(isPersistent, isSystem, #Key, getHashFunc, createDefaultSaveTransactionHelper(getHashFunc), createDefaultSaveSerializedTransactionHelper(getHashFunc), \
                                          [](const QnAbstractTransaction &tran) { return QnTransaction<ParamType>(tran); }, triggerNotificationFunc)

    std::tuple<
               TransactionDescriptor<ApiCommand::tranSyncRequest, ApiSyncRequestData>,
               TransactionDescriptor<ApiCommand::lockRequest, ApiLockData>,
               TransactionDescriptor<ApiCommand::lockResponse, ApiLockData>,
               TransactionDescriptor<ApiCommand::unlockRequest, ApiLockData>,
               TransactionDescriptor<ApiCommand::peerAliveInfo, ApiPeerAliveData>,
               TransactionDescriptor<ApiCommand::tranSyncDone, ApiTranSyncDoneData>,
               TransactionDescriptor<ApiCommand::testConnection, ApiLoginData>,
               TransactionDescriptor<ApiCommand::connect, ApiLoginData>,
               TransactionDescriptor<ApiCommand::openReverseConnection, ApiReverseConnectionData>,
               TransactionDescriptor<ApiCommand::removeResource, ApiIdData>,

               TransactionDescriptor<ApiCommand::setResourceStatus, ApiResourceStatusData>,
               TransactionDescriptor<ApiCommand::setResourceParams, ApiResourceParamWithRefDataList>,
               TransactionDescriptor<ApiCommand::getResourceParams, ApiResourceParamWithRefDataList>,
               TransactionDescriptor<ApiCommand::getResourceTypes, ApiResourceTypeDataList>,
               TransactionDescriptor<ApiCommand::getFullInfo, ApiFullInfoData>,
               TransactionDescriptor<ApiCommand::setResourceParam, ApiResourceParamWithRefData>,
               TransactionDescriptor<ApiCommand::removeResourceParam, ApiResourceParamWithRefData>,
               TransactionDescriptor<ApiCommand::removeResourceParams, ApiResourceParamWithRefDataList>,
               TransactionDescriptor<ApiCommand::getStatusList, ApiResourceStatusDataList>,
               TransactionDescriptor<ApiCommand::removeResources, ApiIdDataList>,
               TransactionDescriptor<ApiCommand::getCameras, ApiCameraDataList>,

               TransactionDescriptor<ApiCommand::saveCamera, ApiCameraData>,
               TransactionDescriptor<ApiCommand::saveCameras, ApiCameraDataList>,
               TransactionDescriptor<ApiCommand::removeCamera, ApiIdData>,
               TransactionDescriptor<ApiCommand::getCameraHistoryItems, ApiServerFootageDataList>,
               TransactionDescriptor<ApiCommand::addCameraHistoryItem, ApiServerFootageData>,
               TransactionDescriptor<ApiCommand::removeCameraHistoryItem, ApiServerFootageData>,
               TransactionDescriptor<ApiCommand::saveCameraUserAttributes, ApiCameraAttributesData>,
               TransactionDescriptor<ApiCommand::saveCameraUserAttributesList, ApiCameraAttributesDataList>,
               TransactionDescriptor<ApiCommand::getCameraUserAttributes, ApiCameraAttributesDataList>,
               TransactionDescriptor<ApiCommand::getCamerasEx, ApiCameraDataExList>,

               TransactionDescriptor<ApiCommand::getMediaServers, ApiMediaServerDataList>,
               TransactionDescriptor<ApiCommand::saveMediaServer, ApiMediaServerData>,
               TransactionDescriptor<ApiCommand::removeMediaServer, ApiIdData>,
               TransactionDescriptor<ApiCommand::saveServerUserAttributes, ApiMediaServerUserAttributesData>,
               TransactionDescriptor<ApiCommand::saveServerUserAttributesList, ApiMediaServerUserAttributesDataList>,
               TransactionDescriptor<ApiCommand::getServerUserAttributes, ApiMediaServerUserAttributesDataList>,
               TransactionDescriptor<ApiCommand::removeServerUserAttributes, ApiIdData>,
               TransactionDescriptor<ApiCommand::saveStorage, ApiStorageData>,

               TransactionDescriptor<ApiCommand::saveStorages, ApiStorageDataList>,
               TransactionDescriptor<ApiCommand::removeStorage, ApiIdData>,
               TransactionDescriptor<ApiCommand::removeStorages, ApiIdDataList>,
               TransactionDescriptor<ApiCommand::getMediaServersEx, ApiMediaServerDataExList>,
               TransactionDescriptor<ApiCommand::getStorages, ApiStorageDataList>,
               TransactionDescriptor<ApiCommand::getUsers, ApiUserDataList>,
               TransactionDescriptor<ApiCommand::saveUser, ApiUserData>,
               TransactionDescriptor<ApiCommand::removeUser, ApiIdData>,

               TransactionDescriptor<ApiCommand::getAccessRights, ApiAccessRightsDataList>,
               TransactionDescriptor<ApiCommand::setAccessRights, ApiAccessRightsData>,
               TransactionDescriptor<ApiCommand::getLayouts, ApiLayoutDataList>,
               TransactionDescriptor<ApiCommand::saveLayout, ApiLayoutData>,
               TransactionDescriptor<ApiCommand::saveLayouts, ApiLayoutDataList>,
               TransactionDescriptor<ApiCommand::removeLayout, ApiIdData>,
               TransactionDescriptor<ApiCommand::getVideowalls, ApiVideowallDataList>,
               TransactionDescriptor<ApiCommand::saveVideowall, ApiVideowallData>,

               TransactionDescriptor<ApiCommand::removeVideowall, ApiIdData>,
               TransactionDescriptor<ApiCommand::videowallControl, ApiVideowallControlMessageData>,
               TransactionDescriptor<ApiCommand::getBusinessRules, ApiBusinessRuleDataList>,
               TransactionDescriptor<ApiCommand::saveBusinessRule, ApiBusinessRuleData>,
               TransactionDescriptor<ApiCommand::removeBusinessRule, ApiIdData>,
               TransactionDescriptor<ApiCommand::resetBusinessRules, ApiResetBusinessRuleData>,
               TransactionDescriptor<ApiCommand::broadcastBusinessAction, ApiBusinessActionData>,
               TransactionDescriptor<ApiCommand::execBusinessAction, ApiBusinessActionData>,

               TransactionDescriptor<ApiCommand::removeStoredFile, ApiStoredFilePath>,
               TransactionDescriptor<ApiCommand::getStoredFile, ApiStoredFileData>,
               TransactionDescriptor<ApiCommand::addStoredFile, ApiStoredFileData>,
               TransactionDescriptor<ApiCommand::updateStoredFile, ApiStoredFileData>,
               TransactionDescriptor<ApiCommand::listDirectory, ApiStoredDirContents>,
               TransactionDescriptor<ApiCommand::getStoredFiles, ApiStoredFileDataList>,
               TransactionDescriptor<ApiCommand::getLicenses, ApiLicenseDataList>,
               TransactionDescriptor<ApiCommand::addLicense, ApiLicenseData>,

               TransactionDescriptor<ApiCommand::addLicenses, ApiLicenseDataList>,
               TransactionDescriptor<ApiCommand::removeLicense, ApiLicenseData>,
               TransactionDescriptor<ApiCommand::uploadUpdate, ApiUpdateUploadData>,
               TransactionDescriptor<ApiCommand::uploadUpdateResponce, ApiUpdateUploadResponceData>,
               TransactionDescriptor<ApiCommand::installUpdate, ApiUpdateInstallData>,
               TransactionDescriptor<ApiCommand::discoveredServerChanged, ApiDiscoveredServerData>,
               TransactionDescriptor<ApiCommand::discoveredServersList, ApiDiscoveredServerDataList>,
               TransactionDescriptor<ApiCommand::discoverPeer, ApiDiscoverPeerData>,

               TransactionDescriptor<ApiCommand::addDiscoveryInformation, ApiDiscoveryData>,
               TransactionDescriptor<ApiCommand::removeDiscoveryInformation, ApiDiscoveryData>,
               TransactionDescriptor<ApiCommand::getDiscoveryData, ApiDiscoveryDataList>,
               TransactionDescriptor<ApiCommand::getWebPages, ApiWebPageDataList>,
               TransactionDescriptor<ApiCommand::saveWebPage, ApiWebPageData>,
               TransactionDescriptor<ApiCommand::removeWebPage, ApiIdData>,
               TransactionDescriptor<ApiCommand::forcePrimaryTimeServer, ApiIdData>,
               TransactionDescriptor<ApiCommand::broadcastPeerSystemTime, ApiPeerSystemTimeData>,

               TransactionDescriptor<ApiCommand::getCurrentTime, ApiTimeData>,
               TransactionDescriptor<ApiCommand::changeSystemName, ApiSystemNameData>,
               TransactionDescriptor<ApiCommand::getKnownPeersSystemTime, ApiPeerSystemTimeDataList>,
               TransactionDescriptor<ApiCommand::markLicenseOverflow, ApiLicenseOverflowData>,
               TransactionDescriptor<ApiCommand::getSettings, ApiResourceParamDataList>,
               TransactionDescriptor<ApiCommand::getClientInfos, ApiClientInfoDataList>,
               TransactionDescriptor<ApiCommand::saveClientInfo, ApiClientInfoData>,
               TransactionDescriptor<ApiCommand::getStatisticsReport, ApiSystemStatistics>,

               TransactionDescriptor<ApiCommand::triggerStatisticsReport, ApiStatisticsServerInfo>,
               TransactionDescriptor<ApiCommand::runtimeInfoChanged, ApiRuntimeData>,
               TransactionDescriptor<ApiCommand::dumpDatabase, ApiDatabaseDumpData>,
               TransactionDescriptor<ApiCommand::restoreDatabase, ApiDatabaseDumpData>,
               TransactionDescriptor<ApiCommand::updatePersistentSequence, ApiUpdateSequenceData>,
               TransactionDescriptor<ApiCommand::dumpDatabaseToFile, ApiDatabaseDumpToFileData>,
               TransactionDescriptor<ApiCommand::getTransactionLog, ApiTransactionDataList>
    > transactionDescriptors =
     std::make_tuple (
//        TRANSACTION_DESCRIPTOR(ApiCommand::NotDefined, NotDefinedType,
//                               true, // persistent
//                               false, // system
//                               InvalidGetHashHelper(), // getHash
//                               [] (const QnTransaction<NotDefinedType> &, NotificationParams &)  // trigger notification
//                                  {
//                                      NX_ASSERT(0, Q_FUNC_INFO, "No notification for this transaction");
//                                  }
//                               ),
        TRANSACTION_DESCRIPTOR(ApiCommand::tranSyncRequest, ApiSyncRequestData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::lockRequest, ApiLockData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::lockResponse, ApiLockData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::unlockRequest, ApiLockData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::peerAliveInfo, ApiPeerAliveData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::tranSyncDone, ApiTranSyncDoneData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::testConnection, ApiLoginData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::connect, ApiLoginData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::openReverseConnection, ApiReverseConnectionData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               [](const QnTransaction<ApiReverseConnectionData> &tran, const NotificationParams &notificationParams)
                               {
                                    NX_ASSERT(tran.command == ApiCommand::openReverseConnection);
                                    emit notificationParams.ecConnection->reverseConnectionRequested(tran.params);
                               } // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeResource, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::setResourceStatus, ApiResourceStatusData,
                               true, // persistent
                               false, // system
                               CreateHashByIdRfc4122Helper(), // getHash
                               ResourceNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::setResourceParams, ApiResourceParamWithRefDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               ResourceNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getResourceParams, ApiResourceParamWithRefDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               ResourceNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getResourceTypes, ApiResourceTypeDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getFullInfo, ApiFullInfoData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               [] (const QnTransaction<ApiFullInfoData> & tran, const NotificationParams &notificationParams)
                               {
                                   emit notificationParams.ecConnection->initNotification(tran.params);
                                   for(const ApiDiscoveryData& data: tran.params.discoveryData)
                                       notificationParams.discoveryNotificationManager->triggerNotification(data);
                               }
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::setResourceParam, ApiResourceParamWithRefData,
                               true, // persistent
                               false, // system
                               CreateHashForResourceParamWithRefDataHelper(), // getHash
                               ResourceNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeResourceParam, ApiResourceParamWithRefData,
                               true, // persistent
                               false, // system
                               CreateHashForResourceParamWithRefDataHelper(), // getHash
                               ResourceNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeResourceParams, ApiResourceParamWithRefDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               ResourceNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getStatusList, ApiResourceStatusDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeResources, ApiIdDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               &apiIdDataListTriggerNotificationHelper
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getCameras, ApiCameraDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               CameraNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveCamera, ApiCameraData,
                               false, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               CameraNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveCameras, ApiCameraDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               [] (const QnTransaction<ApiCameraDataList> &tran, const NotificationParams &notificationParams)
                                 {
                                    notificationParams.cameraNotificationManager->triggerNotification(tran);
                                 }
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeCamera, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getCameraHistoryItems, ApiServerFootageDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::addCameraHistoryItem, ApiServerFootageData,
                               true, // persistent
                               false, // system
                               &createHashForServerFootageDataHelper, // getHash
                               CameraNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeCameraHistoryItem, ApiServerFootageData,
                               true, // persistent
                               false, // system
                               &createHashForServerFootageDataHelper, // getHash
                               CameraNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveCameraUserAttributes, ApiCameraAttributesData,
                               true, // persistent
                               false, // system
                               &createHashForApiCameraAttributesDataHelper, // getHash
                               CameraNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveCameraUserAttributesList, ApiCameraAttributesDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               CameraNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getCameraUserAttributes, ApiCameraAttributesDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               CameraNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getCamerasEx, ApiCameraDataExList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getMediaServers, ApiMediaServerDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveMediaServer, ApiMediaServerData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeMediaServer, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveServerUserAttributes, ApiMediaServerUserAttributesData,
                               true, // persistent
                               false, // system
                               &createHashForApiMediaServerUserAttributesDataHelper, // getHash
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveServerUserAttributesList, ApiMediaServerUserAttributesDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getServerUserAttributes, ApiMediaServerUserAttributesDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeServerUserAttributes, ApiIdData,
                               false, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveStorage, ApiStorageData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveStorages, ApiStorageDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeStorage, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeStorages, ApiIdDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               &apiIdDataListTriggerNotificationHelper
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getMediaServersEx, ApiMediaServerDataExList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getStorages, ApiStorageDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getUsers, ApiUserDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveUser, ApiUserData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               UserNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeUser, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getAccessRights, ApiAccessRightsDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()// trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::setAccessRights, ApiAccessRightsData,
                               true, // persistent
                               false, // system
                               CreateHashByUserIdHelper(), // getHash
                               UserNotificationManagerHelper()// trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getLayouts, ApiLayoutDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()// trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveLayout, ApiLayoutData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               LayoutNotificationManagerHelper()// trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveLayouts, ApiLayoutDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               LayoutNotificationManagerHelper()// trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeLayout, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getVideowalls, ApiVideowallDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveVideowall, ApiVideowallData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               VideowallNotificationManagerHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeVideowall, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::videowallControl, ApiVideowallControlMessageData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               VideowallNotificationManagerHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getBusinessRules, ApiBusinessRuleDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveBusinessRule, ApiBusinessRuleData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               BusinessEventNotificationManagerHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeBusinessRule, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::resetBusinessRules, ApiResetBusinessRuleData,
                               true, // persistent
                               false, // system
                               [] (const ApiResetBusinessRuleData&) { return QnTransactionLog::makeHash("reset_brule", ADD_HASH_DATA); }, // getHash
                               BusinessEventNotificationManagerHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::broadcastBusinessAction, ApiBusinessActionData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               BusinessEventNotificationManagerHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::execBusinessAction, ApiBusinessActionData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               BusinessEventNotificationManagerHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeStoredFile, ApiStoredFilePath,
                               true, // persistent
                               false, // system
                               [] (const ApiStoredFilePath &params) { return QnTransactionLog::makeHash(params.path.toUtf8()); }, // getHash
                               [] (const QnTransaction<ApiStoredFilePath> &tran, const NotificationParams &notificationParams)
                                {
                                    NX_ASSERT(tran.command == ApiCommand::removeStoredFile);
                                    notificationParams.storedFileNotificationManager->triggerNotification(tran);
                                }
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getStoredFile, ApiStoredFileData,
                               false, // persistent
                               false, // system
                               &createHashForApiStoredFileDataHelper, // getHash
                               StoredFileNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::addStoredFile, ApiStoredFileData,
                               true, // persistent
                               false, // system
                               &createHashForApiStoredFileDataHelper, // getHash
                               StoredFileNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::updateStoredFile, ApiStoredFileData,
                               true, // persistent
                               false, // system
                               &createHashForApiStoredFileDataHelper, // getHash
                               StoredFileNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::listDirectory, ApiStoredDirContents,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getStoredFiles, ApiStoredFileDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getLicenses, ApiLicenseDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               LicenseNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::addLicense, ApiLicenseData,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::addLicenses, ApiLicenseDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               LicenseNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeLicense, ApiLicenseData,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::uploadUpdate, ApiUpdateUploadData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               UpdateNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::uploadUpdateResponce, ApiUpdateUploadResponceData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               UpdateNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::installUpdate, ApiUpdateInstallData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               UpdateNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::discoveredServerChanged, ApiDiscoveredServerData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               DiscoveryNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::discoveredServersList, ApiDiscoveredServerDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               DiscoveryNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::discoverPeer, ApiDiscoverPeerData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               DiscoveryNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::addDiscoveryInformation, ApiDiscoveryData,
                               true, // persistent
                               false, // system
                               &createHashForApiDiscoveryDataHelper, // getHash
                               DiscoveryNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeDiscoveryInformation, ApiDiscoveryData,
                               true, // persistent
                               false, // system
                               &createHashForApiDiscoveryDataHelper, // getHash
                               DiscoveryNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getDiscoveryData, ApiDiscoveryDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               DiscoveryNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getWebPages, ApiWebPageDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveWebPage, ApiWebPageData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               WebPageNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeWebPage, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::forcePrimaryTimeServer, ApiIdData,
                               false, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               &apiIdDataTriggerNotificationHelper // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::broadcastPeerSystemTime, ApiPeerSystemTimeData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               EmptyNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getCurrentTime, ApiTimeData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::changeSystemName, ApiSystemNameData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               [] (const QnTransaction<ApiSystemNameData> &tran, const NotificationParams &notificationParams)
                                { return notificationParams.miscNotificationManager->triggerNotification(tran); }// trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getKnownPeersSystemTime, ApiPeerSystemTimeDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::markLicenseOverflow, ApiLicenseOverflowData,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               ResourceNotificationManagerHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getSettings, ApiResourceParamDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getClientInfos, ApiClientInfoDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveClientInfo, ApiClientInfoData,
                               true, // persistent
                               false, // system
                               CreateHashByIdRfc4122Helper(), // getHash
                               EmptyNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getStatisticsReport, ApiSystemStatistics,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::triggerStatisticsReport, ApiStatisticsServerInfo,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper() // trigger notification
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::runtimeInfoChanged, ApiRuntimeData,
                               false, // persistent
                               true, // system
                               InvalidGetHashHelper(), // getHash
                               [] (const QnTransaction<ApiRuntimeData> & tran, const NotificationParams &notificationParams)
                                {
                                    NX_ASSERT(tran.command == ApiCommand::runtimeInfoChanged);
                                    emit notificationParams.ecConnection->runtimeInfoChanged(tran.params);
                                }
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::dumpDatabase, ApiDatabaseDumpData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               [] (const QnTransaction<ApiDatabaseDumpData> &, const NotificationParams &notificationParams)
                                {
                                    emit notificationParams.ecConnection->databaseDumped();
                                }
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::restoreDatabase, ApiDatabaseDumpData,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               [] (const QnTransaction<ApiDatabaseDumpData> &, const NotificationParams &notificationParams)
                                {
                                    emit notificationParams.ecConnection->databaseDumped();
                                }
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::updatePersistentSequence, ApiUpdateSequenceData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               EmptyNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::dumpDatabaseToFile, ApiDatabaseDumpToFileData,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               EmptyNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getTransactionLog, ApiTransactionDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               InvalidTriggerNotificationHelper()
                              )
      );
    #undef TRANSACTION_DESCRIPTOR
    } // namespace detail
    } // namespace ec2

    struct GenericVisitor
    {
        template<typename Desc>
        void operator()(const Desc &desc)
        {
            std::cout << desc.isPersistent;
        }
    };

    int foo()
    {
//        std::cout << ec2::getTransactionDescriptorByValue<ec2::ApiCommand::Value::NotDefined>().isPersistent;
        std::cout << ec2::getTransactionDescriptorByTransactionParams<ec2::ApiSyncRequestData>().isPersistent;
        auto tuple = ec2::getTransactionDescriptorsFilteredByTransactionParams<ec2::ApiSyncRequestData>();
        ec2::visitTransactionDescriptorIfValue(ec2::ApiCommand::addLicense, GenericVisitor(), tuple);
//        ec2::visitTransactionDescriptorIfValue(ec2::ApiCommand::NotDefined, GenericVisitor());
        return 1;
    }
