#include <iostream>
#include "transaction_descriptor.h"
#include "transaction_message_bus.h"
#include "nx_ec/data/api_tran_state_data.h"

namespace ec2
{
namespace detail
{
    template<typename Param>
    auto getApiIdImpl(const Param &p, int) -> nx::utils::SfinaeCheck<decltype(p.id), QnUuid>
    {
        return p.id;
    }

    template<typename Param>
    auto getApiIdImpl(const Param &, char) -> QnUuid
    {
        return QnUuid();
    }

    template<typename Param>
    QnUuid getApiId(const QnTransaction<Param> &tran)
    {
        return getApiIdImpl(tran.params, 0);
    }

    template<typename T, typename F>
    ErrorCode saveTransactionImpl(const QnTransaction<T>& tran, ec2::QnTransactionLog *tlog, F f)
    {
        QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
        return tlog->saveToDB(tran, getApiId(tran), f(tran.params), serializedTran);
    }

    template<typename T, typename F>
    ErrorCode saveSerializedTransactionImpl(const QnTransaction<T>& tran, const QByteArray& serializedTran, QnTransactionLog *tlog, F f)
    {
        return tlog->saveToDB(tran, getApiId(tran), f(tran.params), serializedTran);
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
        case ApiCommand::removeUserGroup:
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

    QnUuid createHashForApiLicenseDataHelper(const ApiLicenseData &params)
    {
        return QnTransactionLog::makeHash(params.key, "ApiLicense");
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

#define TRANSACTION_DESCRIPTOR_APPLY( \
    _, \
    Key, \
    ParamType, \
    isPersistent, \
    isSystem, \
    getHashFunc, \
    triggerNotificationFunc \
    ) \
    std::make_shared<TransactionDescriptor<ParamType>>( \
        ApiCommand::Key, \
        isPersistent, \
        isSystem, \
        #Key, \
        getHashFunc, \
        createDefaultSaveTransactionHelper(getHashFunc), \
        createDefaultSaveSerializedTransactionHelper(getHashFunc), \
        [](const QnAbstractTransaction &tran) { return QnTransaction<ParamType>(tran); },  \
        triggerNotificationFunc),


DescriptorBaseContainer transactionDescriptors = {
    TRANSACTION_DESCRIPTOR_LIST(TRANSACTION_DESCRIPTOR_APPLY)
};

#undef TRANSACTION_DESCRIPTOR_APPLY
} // namespace detail

detail::TransactionDescriptorBase *getTransactionDescriptorByValue(ApiCommand::Value v)
{
    auto it = detail::transactionDescriptors.get<0>().find(v);
    bool isEnd = it == detail::transactionDescriptors.get<0>().end();
    NX_ASSERT(!isEnd, "ApiCommand::Value not found");
    return (*it).get();
}

detail::TransactionDescriptorBase *getTransactionDescriptorByName(const QString &s)
{
    auto it = detail::transactionDescriptors.get<1>().find(s);
    bool isEnd = it == detail::transactionDescriptors.get<1>().end();
    NX_ASSERT(!isEnd, "ApiCommand name not found");
    return (*it).get();
}

} // namespace ec2
