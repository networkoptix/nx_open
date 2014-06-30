#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <vector>

#ifndef QN_NO_QT
#include <QtCore/QString>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_email_data.h"
#endif

#include "utils/serialization/binary.h"

namespace ec2
{
    namespace ApiCommand
    {
        // TODO: #Elric #enum
        enum Value
        {
		    NotDefined                  = 0,

            /* System */
            tranSyncRequest             = 1,    /*< QnTranState */
            tranSyncResponse            = 2,    /*< QnTranStateResponse */       
            lockRequest                 = 3,    /*< ApiLockData */
            lockResponse                = 4,    /*< ApiLockData */
            unlockRequest               = 5,    /*< ApiLockData */
            peerAliveInfo               = 6,    /*< ApiPeerAliveData */

            /* Connection */
            testConnection              = 100,  /*< ApiLoginData */
            connect                     = 101,  /*< ApiLoginData */

            /* Common resource */
            saveResource                = 200,  /*< ApiResourceData */
            removeResource              = 201,  /*< ApiIdData */
            setResourceStatus           = 202,  /*< ApiSetResourceStatusData */
            getResourceParams           = 203,  /*< ApiResourceParamDataList */
            setResourceParams           = 204,  /*< ApiResourceParamsData */
            getResourceTypes            = 205,  /*< ApiResourceTypeDataList*/
            getFullInfo                 = 206,  /*< ApiFullInfoData */
            setPanicMode                = 207,  /*< ApiPanicModeData */

            /* Camera resource */
            getCameras                  = 300,  /*< ApiCameraDataList */
            saveCamera                  = 301,  /*< ApiCameraData */
            saveCameras                 = 302,  /*< ApiCameraDataList */
            removeCamera                = 303,  /*< ApiIdData */
            getCameraHistoryItems       = 304,  /*< ApiCameraServerItemDataList */
            addCameraHistoryItem        = 305,  /*< ApiCameraServerItemData */
            addCameraBookmarkTags       = 306,  /*< ApiCameraBookmarkTagDataList */
            getCameraBookmarkTags       = 307,  /*< ApiCameraBookmarkTagDataList */
            removeCameraBookmarkTags    = 308,
            
          
            /* MediaServer resource */
            getMediaServers             = 400,  /*< ApiMediaServerDataList */
            saveMediaServer             = 401,  /*< ApiMediaServerData */
            removeMediaServer           = 402,  /*< ApiIdData */

            /* User resource */
            getUsers                    = 500,  /*< ApiUserDataList */
            saveUser                    = 501,  /*< ApiUserData */
            removeUser                  = 502,  /*< ApiIdData */

            /* Layout resource */
            getLayouts                  = 600,  /*< ApiLayoutDataList */
            saveLayout                  = 601,  /*< ApiLayoutDataList */
            saveLayouts                 = 602,  /*< ApiLayoutData */
            removeLayout                = 603,  /*< ApiIdData */

            /* Videowall resource */
            getVideowalls               = 700,  /*< ApiVideowallDataList */
            saveVideowall               = 701,  /*< ApiVideowallData */
            removeVideowall             = 702,  /*< ApiIdData */
            videowallControl            = 703,  /*< ApiVideowallControlMessageData */
            videowallInstanceStatus     = 704,  /*< ApiVideowallInstanceStatusData */

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

            /* Licenses */
            getLicenses                 = 1000, /*< ApiLicenseDataList */
            addLicense                  = 1001, /*< ApiLicenseData */
            addLicenses                 = 1002, /*< ApiLicenseDataList */

            /* Email */
            testEmailSettings           = 1100, /*< ApiEmailSettingsData */
            sendEmail                   = 1101, /*< ApiEmailData */

            /* Auto-updates */
            uploadUpdate                = 1200, /*< ApiUpdateUploadData */
            uploadUpdateResponce        = 1201, /*< ApiUpdateUploadResponceData */
            installUpdate               = 1202, /*< ApiUpdateInstallData  */

            /* Misc */
            getSettings                 = 9000,  /*< ApiResourceParamDataList */
            saveSettings                = 9001,  /*< ApiResourceParamDataList */
            getCurrentTime              = 9002,  /*< ApiTimeData */         
            getHelp                     = 9003,  /*< ApiHelpGroupDataList */
			runtimeInfoChanged          = 9004,  /*< ApiRuntimeData */

			maxTransactionValue         = 65535
        };
        QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Value)

        QString toString( Value val );
        bool isSystem( Value val );
    }

    class QnAbstractTransaction
    {
    public:
		QnAbstractTransaction(): command(ApiCommand::NotDefined), persistent(false), timestamp(0), isLocal(false) {}
        QnAbstractTransaction(ApiCommand::Value command, bool persistent);
        
        static void setStartSequence(int value);
        void fillSequence();

        struct ID
        {
            ID(): sequence(0) {}
            QUuid peerID;
            QUuid dbID;
            qint32 sequence;

#ifndef QN_NO_QT
            friend uint qHash(const ec2::QnAbstractTransaction::ID &id) {
                return ::qHash(id.peerID.toRfc4122().append(id.dbID.toRfc4122()), id.sequence);
            }
#endif

            bool operator==(const ID &other) const {
                return peerID == other.peerID && dbID == other.dbID && sequence == other.sequence;
            }
        };

        ApiCommand::Value command;
        ID id;
        bool persistent;
        qint64 timestamp;
        
        bool isLocal; // do not propagate transactions to other server peers
    };

    typedef QnAbstractTransaction::ID QnAbstractTransaction_ID;
#define QnAbstractTransaction_ID_Fields (peerID)(dbID)(sequence)
#define QnAbstractTransaction_Fields (command)(id)(persistent)(timestamp)        

    template <class T>
    class QnTransaction: public QnAbstractTransaction
    {
    public:
        QnTransaction() {}
        QnTransaction(const QnAbstractTransaction& abstractTran): QnAbstractTransaction(abstractTran) {}
        QnTransaction(ApiCommand::Value command, bool persistent): QnAbstractTransaction(command, persistent) {}
        
        T params;
    };

    QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction::ID, (binary)(json))
    QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction, (binary)(json))

    template <class T, class Output>
    void serialize(const QnTransaction<T> &transaction,  QnOutputBinaryStream<Output> *stream)
    {
        QnBinary::serialize(static_cast<const QnAbstractTransaction &>(transaction), stream);
        QnBinary::serialize(transaction.params, stream);
    }

    template <class T, class Input>
    bool deserialize(QnInputBinaryStream<Input>* stream,  QnTransaction<T> *transaction)
    {
        return 
            QnBinary::deserialize(stream,  static_cast<QnAbstractTransaction *>(transaction)) &&
            QnBinary::deserialize(stream, &transaction->params);
    }

    int generateRequestID();
} // namespace ec2

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ec2::ApiCommand::Value), (numeric))

#ifndef QN_NO_QT
Q_DECLARE_METATYPE(ec2::QnAbstractTransaction)
#endif

#endif  //EC2_TRANSACTION_H
