#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <vector>

#ifndef QN_NO_QT
#include <QtCore/QString>
#include "nx_ec/ec_api.h"
#endif

#include "utils/serialization/binary.h"
#include "utils/serialization/json.h"
#include "utils/serialization/xml.h"
#include "utils/serialization/csv.h"
#include "utils/serialization/ubjson.h"
#include "common/common_module.h"

namespace ec2
{
    namespace ApiCommand
    {
        // TODO: #Elric #enum
        enum Value
        {
            NotDefined                  = 0,

            /* System */
            tranSyncRequest             = 1,    /*< ApiSyncRequestData */
            tranSyncResponse            = 2,    /*< QnTranStateResponse */
            lockRequest                 = 3,    /*< ApiLockData */
            lockResponse                = 4,    /*< ApiLockData */
            unlockRequest               = 5,    /*< ApiLockData */
            peerAliveInfo               = 6,    /*< ApiPeerAliveData */
            tranSyncDone                = 7,    /*< ApiTranSyncDoneData */

            /* Connection */
            testConnection              = 100,  /*< ApiLoginData */
            connect                     = 101,  /*< ApiLoginData */
            openReverseConnection       = 102,  /*< ApiReverseConnectionData */

            /* Common resource */
            saveResource                = 200,  /*< ApiResourceData */
            removeResource              = 201,  /*< ApiIdData */
            setResourceStatus           = 202,  /*< ApiResourceStatusData */
            getResourceParams           = 203,  /*< ApiResourceParamDataList */
            setResourceParams           = 204,  /*< ApiResourceParamWithRefDataList */
            getResourceTypes            = 205,  /*< ApiResourceTypeDataList*/
            getFullInfo                 = 206,  /*< ApiFullInfoData */
            setResourceParam            = 208,   /*< ApiResourceParamWithRefData */
            removeResourceParam         = 209,   /*< ApiResourceParamWithRefData */
            removeResourceParams        = 210,   /*< ApiResourceParamWithRefDataList */
            getStatusList               = 211,  /*< ApiResourceStatusDataList */
            removeResources             = 212,  /*< ApiIdDataList */

            /* Camera resource */
            getCameras                  = 300,  /*< ApiCameraDataList */
            saveCamera                  = 301,  /*< ApiCameraData */
            saveCameras                 = 302,  /*< ApiCameraDataList */
            removeCamera                = 303,  /*< ApiIdData */
            getCameraHistoryItems       = 304,  /*< ApiServerFootageDataList */
            addCameraHistoryItem        = 305,  /*< ApiServerFootageData */
            removeCameraHistoryItem     = 309,  /*< ApiServerFootageData */
            saveCameraUserAttributes    = 310,  /*< ApiCameraAttributesData */
            saveCameraUserAttributesList= 311,  /*< ApiCameraAttributesDataList */
            getCameraUserAttributes     = 312,  /*< ApiCameraAttributesDataList */
            getCamerasEx                = 313,  /*< ApiCameraDataExList */


            /* MediaServer resource */
            getMediaServers             = 400,  /*< ApiMediaServerDataList */
            saveMediaServer             = 401,  /*< ApiMediaServerData */
            removeMediaServer           = 402,  /*< ApiIdData */
            saveServerUserAttributes    = 403,  /*< QnMediaServerUserAttributesList */
            saveServerUserAttributesList= 404,  /*< QnMediaServerUserAttributesList */
            getServerUserAttributes     = 405,  /*< ApiIdData, QnMediaServerUserAttributesList */
            removeServerUserAttributes  = 406,  /*< ApiIdData */
            saveStorage                 = 407,  /*< ApiStorageData */
            saveStorages                = 408,  /*< ApiStorageDataList */
            removeStorage               = 409,  /*< ApiIdData */
            removeStorages              = 410,  /*< QList<ApiIdData> */
            getMediaServersEx           = 411,  /*< ApiMediaServerDataExList */
            getStorages                 = 412,  /*< ApiStorageDataList */

            /* User resource */
            getUsers                    = 500,  /*< ApiUserDataList */
            saveUser                    = 501,  /*< ApiUserData */
            removeUser                  = 502,  /*< ApiIdData */

            /* Layout resource */
            getLayouts                  = 600,  /*< ApiLayoutDataList */
            saveLayout                  = 601,  /*< ApiLayoutData */
            saveLayouts                 = 602,  /*< ApiLayoutDataList */
            removeLayout                = 603,  /*< ApiIdData */

            /* Videowall resource */
            getVideowalls               = 700,  /*< ApiVideowallDataList */
            saveVideowall               = 701,  /*< ApiVideowallData */
            removeVideowall             = 702,  /*< ApiIdData */
            videowallControl            = 703,  /*< ApiVideowallControlMessageData */

            /* Business rules */
            getBusinessRules            = 800,  /*< ApiBusinessRuleDataList */
            saveBusinessRule            = 801,  /*< ApiBusinessRuleData */
            removeBusinessRule          = 802,  /*< ApiIdData */
            resetBusinessRules          = 803,  /*< ApiResetBusinessRuleData */
            broadcastBusinessAction     = 804,  /*< ApiBusinessActionData */
            execBusinessAction          = 805,  /*< ApiBusinessActionData */

            /* Stored files */
            listDirectory               = 900,  /*< ApiStoredDirContents */
            getStoredFile               = 901,  /*< ApiStoredFileData */
            addStoredFile               = 902,  /*< ApiStoredFileData */
            updateStoredFile            = 903,  /*< ApiStoredFileData */
            removeStoredFile            = 904,  /*< ApiStoredFilePath */
            getStoredFiles              = 905,  /*< ApiStoredFileDataList */

            /* Licenses */
            getLicenses                 = 1000, /*< ApiLicenseDataList */
            addLicense                  = 1001, /*< ApiLicenseData */
            addLicenses                 = 1002, /*< ApiLicenseDataList */
            removeLicense               = 1003, /*< ApiLicenseData */

            /* Email */
            testEmailSettings           = 1100, /*< ApiEmailSettingsData */
            sendEmail                   = 1101, /*< ApiEmailData */

            /* Auto-updates */
            uploadUpdate                = 1200, /*< ApiUpdateUploadData */
            uploadUpdateResponce        = 1201, /*< ApiUpdateUploadResponceData */
            installUpdate               = 1202, /*< ApiUpdateInstallData  */

            /* Module information */
            discoveredServerChanged     = 1301, /*< ApiDiscoveredServerData */
            discoveredServersList       = 1302, /*< ApiDiscoveredServerDataList */

            /* Discovery */
            discoverPeer                = 1401, /*< ApiDiscoveryData */
            addDiscoveryInformation     = 1402, /*< ApiDiscoveryData*/
            removeDiscoveryInformation  = 1403, /*< ApiDiscoveryData*/
            getDiscoveryData            = 1404, /*< ApiDiscoveryDataList */

            /* WebPage resource */
            getWebPages                 = 1500,  /*< ApiWebPageDataList */
            saveWebPage                 = 1501,  /*< ApiWebPageData */
            removeWebPage               = 1502,  /*< ApiIdData */

            /* Misc */
            forcePrimaryTimeServer      = 2001,  /*< ApiIdData */
            broadcastPeerSystemTime     = 2002,  /*< ApiPeerSystemTimeData*/
            getCurrentTime              = 2003,  /*< ApiTimeData */
            changeSystemName            = 2004,  /*< ApiSystemNameData */
            getKnownPeersSystemTime     = 2005,  /*< ApiPeerSystemTimeDataList */
            markLicenseOverflow         = 2006,  /*< ApiLicenseOverflowData */
            getSettings                 = 2007,  /*< ApiResourceParamDataList */

            /* Client information */
            getClientInfos              = 4001,  /*< ApiClientInfoDataList */
            saveClientInfo              = 4002,  /*< ApiClientInfoData */

            /* Statistics */
            getStatisticsReport         = 5001,  /*< ApiSystemStatistics */
            triggerStatisticsReport     = 5002,  /*< ApiStatisticsServerInfo */

            //getHelp                     = 9003,  /*< ApiHelpGroupDataList */
			runtimeInfoChanged          = 9004,  /*< ApiRuntimeData */
            dumpDatabase                = 9005,  /*< ApiDatabaseDumpData */
            restoreDatabase             = 9006,  /*< ApiDatabaseDumpData */
            updatePersistentSequence    = 9009,  /*< ApiUpdateSequenceData*/
            dumpDatabaseToFile          = 9010,  /*< ApiStoredFilePath, size_t (dump size) */

            getTransactionLog           = 10000,  /*< QnAbstractTransactionList*/

            /* Ldap */
            testLdapSettings            = 11001, /*< QnLdapSettings */

            maxTransactionValue         = 65535
        };
        QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Value)

        QString toString( Value val );
        Value fromString(const QString& val);

        /** Check if transaction can be sent independently of current syncronisation state.
         *  These transactions are rather commands then data transfers. */
        bool isSystem( Value val );

        /** Check if transaction should be written to data base. */
        bool isPersistent( Value val );
    }

    //!Contains information on how transaction has been delivered
    class QnTranDeliveryInformation
    {
    public:
        enum TranOriginatorType
        {
            localServer,
            remoteServer,
            client
        };

        TranOriginatorType originatorType;

        QnTranDeliveryInformation()
        :
            originatorType( TranOriginatorType::localServer )
        {
        }
    };

    class QnAbstractTransaction
    {
    public:
		QnAbstractTransaction(): command(ApiCommand::NotDefined), isLocal(false) { peerID = qnCommon->moduleGUID(); }
        QnAbstractTransaction(ApiCommand::Value value): command(value), isLocal(false) { peerID = qnCommon->moduleGUID(); }

        struct PersistentInfo
        {
            PersistentInfo(): sequence(0), timestamp(0) {}
            bool isNull() const { return dbID.isNull(); }

            QnUuid dbID;
            qint32 sequence;
            qint64 timestamp;

#ifndef QN_NO_QT
            friend uint qHash(const ec2::QnAbstractTransaction::PersistentInfo &id) {
                return ::qHash(QByteArray(id.dbID.toRfc4122()).append((const char*)&id.timestamp, sizeof(id.timestamp)), id.sequence);
            }
#endif

            bool operator==(const PersistentInfo &other) const {
                return dbID == other.dbID && sequence == other.sequence && timestamp == other.timestamp;
            }
        };

        ApiCommand::Value command;
        QnUuid peerID;
        PersistentInfo persistentInfo;
        QnTranDeliveryInformation deliveryInfo;

        bool isLocal; // do not propagate transactions to other server peers

        QString toString() const;
    };


    typedef std::vector<ec2::QnAbstractTransaction> QnAbstractTransactionList;
    typedef QnAbstractTransaction::PersistentInfo QnAbstractTransaction_PERSISTENT;
#define QnAbstractTransaction_PERSISTENT_Fields (dbID)(sequence)(timestamp)
#define QnAbstractTransaction_Fields (command)(peerID)(persistentInfo)(isLocal)

    template <class T>
    class QnTransaction: public QnAbstractTransaction
    {
    public:
        QnTransaction(): QnAbstractTransaction() {}
        QnTransaction(const QnAbstractTransaction& abstractTran): QnAbstractTransaction(abstractTran) {}
        QnTransaction(ApiCommand::Value command, const T& params = T()): QnAbstractTransaction(command), params(params) {}

        T params;
    };

    QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction::PersistentInfo, (json)(ubjson)(xml)(csv_record))
    QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction, (json)(ubjson)(xml)(csv_record))

    //Binary format functions for QnTransaction<T>
    //template <class T, class Output>
    //void serialize(const QnTransaction<T> &transaction,  QnOutputBinaryStream<Output> *stream)
    //{
    //    QnBinary::serialize(static_cast<const QnAbstractTransaction &>(transaction), stream);
    //    QnBinary::serialize(transaction.params, stream);
    //}

    //template <class T, class Input>
    //bool deserialize(QnInputBinaryStream<Input>* stream,  QnTransaction<T> *transaction)
    //{
    //    return
    //        QnBinary::deserialize(stream,  static_cast<QnAbstractTransaction *>(transaction)) &&
    //        QnBinary::deserialize(stream, &transaction->params);
    //}


    //Json format functions for QnTransaction<T>
    template<class T>
    void serialize(QnJsonContext* ctx, const QnTransaction<T>& tran, QJsonValue* target)
    {
        QJson::serialize(ctx, static_cast<const QnAbstractTransaction&>(tran), target);
        QJsonObject localTarget = target->toObject();
        QJson::serialize(ctx, tran.params, "params", &localTarget);
        *target = localTarget;
    }

    template<class T>
    bool deserialize(QnJsonContext* ctx, const QJsonValue& value, QnTransaction<T>* target)
    {
        return
            QJson::deserialize(ctx, value, static_cast<QnAbstractTransaction*>(target)) &&
            QJson::deserialize(ctx, value.toObject(), "params", &target->params);
    }

    //QnUbjson format functions for QnTransaction<T>
    template <class T, class Output>
    void serialize(const QnTransaction<T> &transaction,  QnUbjsonWriter<Output> *stream)
    {
        QnUbjson::serialize(static_cast<const QnAbstractTransaction &>(transaction), stream);
        QnUbjson::serialize(transaction.params, stream);
    }

    template <class T, class Input>
    bool deserialize(QnUbjsonReader<Input>* stream,  QnTransaction<T> *transaction)
    {
        return
            QnUbjson::deserialize(stream,  static_cast<QnAbstractTransaction *>(transaction)) &&
            QnUbjson::deserialize(stream, &transaction->params);
    }

    struct ApiTransactionData: public ApiData
    {
        QnUuid tranGuid;
        QnAbstractTransaction tran;
    };
#define ApiTransactionDataFields (tranGuid)(tran)
QN_FUSION_DECLARE_FUNCTIONS(ApiTransactionData, (json)(ubjson)(xml)(csv_record))


    int generateRequestID();
} // namespace ec2

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ec2::ApiCommand::Value), (metatype)(numeric))

#ifndef QN_NO_QT
Q_DECLARE_METATYPE(ec2::QnAbstractTransaction)
#endif

#endif  //EC2_TRANSACTION_H
