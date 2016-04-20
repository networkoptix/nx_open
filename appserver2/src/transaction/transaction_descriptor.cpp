#include <iostream>
#include "transaction_descriptor.h"
#include "transaction_log.h"
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
        return DefaultSaveTransactionHelper<GetHash>(getHash);
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
        void operator ()(const QnTransaction<Param> &, NotificationParams &)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a system transaction!"); // we MUSTN'T be here
        }
    };

    struct CreateHashByIdHelper
    {
        template<typename Param>
        QnUuid operator ()(const Param &param) { return param.id; }
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

    void apiIdDataTriggerNotificationHelper(const QnTransaction<ApiIdData> &tran, NotificationParams &notificationParams)
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
        return makeHash(params.serverID.toRfc4122(), "server_attributes");
    }

    struct CameraNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, NotificationParams &notificationParams)
        {
            notificationParams.cameraNotificationManager->triggerNotification(tran);
        }
    };

    struct ResourceNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, NotificationParams &notificationParams)
        {
            notificationParams.resourceNotificationManager->triggerNotification(tran);
        }
    };

    struct MediaServerNotificationManagerHelper
    {
        template<typename Param>
        void operator ()(const QnTransaction<Param> &tran, NotificationParams &notificationParams)
        {
            notificationParams.mediaServerNotificationManager->triggerNotification(tran);
        }
    };

#define TRANSACTION_DESCRIPTOR(Key, ParamType, isPersistent, isSystem, getHashFunc, triggerNotificationFunc) \
    TransactionDescriptor<Key, ParamType>(isPersistent, isSystem, #Key, getHashFunc, createDefaultSaveTransactionHelper(getHashFunc), createDefaultSaveSerializedTransactionHelper(getHashFunc), \
                                          [](const QnAbstractTransaction &tran) { return QnTransaction<ParamType>(tran); }, triggerNotificationFunc)

    std::tuple<TransactionDescriptor<ApiCommand::NotDefined, NotDefinedType>,
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
               TransactionDescriptor<ApiCommand::setResourceParams, ApiResourceParamDataList>,
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
               TransactionDescriptor<ApiCommand::saveStorage, ApiStorageData>
    > transactionDescriptors =
     std::make_tuple (
        TRANSACTION_DESCRIPTOR(ApiCommand::NotDefined, NotDefinedType,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               [] (const QnTransaction<NotDefinedType> &, NotificationParams &)  // trigger notification
                                  {
                                      NX_ASSERT(0, Q_FUNC_INFO, "No notification for this transaction");
                                  }
                               ),
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
                               [](const QnTransaction<ApiReverseConnectionData> &tran, NotificationParams &notificationParams)
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
        TRANSACTION_DESCRIPTOR(ApiCommand::getResourceParams, ApiResourceParamWithRefDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               ResourceNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::setResourceParams, ApiResourceParamWithRefDataList,
                               true, // persistent
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
                               [] (const QnTransaction<ApiFullInfoData> & tran, NotificationParams &notificationParams)
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
                               [] (const QnTransaction<ApiIdDataList> &tran, NotificationParams &notificationParams)
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
                               [] (const QnTransaction<ApiCameraDataList> &tran, NotificationParams &notificationParams)
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
                               createDefaultSaveTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               CameraNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getCameraUserAttributes, ApiCameraAttributesDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               CameraNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getCamerasEx, ApiCameraDataExList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getMediaServers, ApiMediaServerDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               InvalidTriggerNotificationHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveMediaServer, ApiMediaServerData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               createDefaultSaveTransactionHelper(CreateHashByIdHelper()), // save
                               createDefaultSaveSerializedTransactionHelper(CreateHashByIdHelper()), // save serialized
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeMediaServer, ApiIdData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               createDefaultSaveTransactionHelper(CreateHashByIdHelper()), // save
                               createDefaultSaveSerializedTransactionHelper(CreateHashByIdHelper()), // save serialized
                               &apiIdDataTriggerNotificationHelper
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveServerUserAttributes, ApiMediaServerUserAttributesData,
                               true, // persistent
                               false, // system
                               &createHashForApiMediaServerUserAttributesDataHelper, // getHash
                               createDefaultSaveTransactionHelper(&createHashForApiMediaServerUserAttributesDataHelper), // save
                               createDefaultSaveSerializedTransactionHelper(&createHashForApiMediaServerUserAttributesDataHelper), // save serialized
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveServerUserAttributesList, ApiMediaServerUserAttributesDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::getServerUserAttributes, ApiMediaServerUserAttributesDataList,
                               false, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::removeServerUserAttributes, ApiIdData,
                               false, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               createDefaultSaveTransactionHelper(CreateHashByIdHelper()), // save
                               createDefaultSaveSerializedTransactionHelper(CreateHashByIdHelper()), // save serialized
                               &apiIdDataTriggerNotificationHelper
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveStorage, ApiStorageData,
                               true, // persistent
                               false, // system
                               CreateHashByIdHelper(), // getHash
                               createDefaultSaveTransactionHelper(CreateHashByIdHelper()), // save
                               createDefaultSaveSerializedTransactionHelper(CreateHashByIdHelper()), // save serialized
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveStorages, ApiStorageDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               MediaServerNotificationManagerHelper()
                              ),
        TRANSACTION_DESCRIPTOR(ApiCommand::saveStorages, ApiStorageDataList,
                               true, // persistent
                               false, // system
                               InvalidGetHashHelper(), // getHash
                               createDefaultSaveTransactionHelper(InvalidGetHashHelper()), // save
                               InvalidSaveSerializedTransactionHelper(), // save serialized
                               MediaServerNotificationManagerHelper()
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
    std::cout << ec2::getTransactionDescriptorByValue<ec2::ApiCommand::Value::NotDefined>().isPersistent;
    std::cout << ec2::getTransactionDescriptorByTransactionParams<ec2::ApiSyncRequestData>().isPersistent;
    auto tuple = ec2::getTransactionDescriptorsFilteredByTransactionParams<ec2::ApiSyncRequestData>();
    ec2::visitTransactionDescriptorIfValue(ec2::ApiCommand::addLicense, GenericVisitor(), tuple);
    ec2::visitTransactionDescriptorIfValue(ec2::ApiCommand::NotDefined, GenericVisitor());
    return 1;
}
