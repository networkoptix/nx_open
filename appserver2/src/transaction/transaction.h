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
		    NotDefined = 0,

            testConnection = 1,
            connect = 2,

            //!ApiResourceTypeList
            getResourceTypes = 4,
            //!ApiResource
            getResource = 5,
            //!ApiSetResourceStatusData
            setResourceStatus = 6,
            //!ApiSetResourceDisabledData
            //setResourceDisabled = 7,
            //!ApiResourceParams
            setResourceParams = 8,
            getResourceParams = 9,
            //!ApiResource
            saveResource = 10,
            removeResource = 11,
            //!ApiPanicModeData
            setPanicMode = 12,
            //!ApiFullInfo,
            getFullInfo = 13,
            
            //!ApiCameraData
            saveCamera = 14,
            //!ApiCameraList
            saveCameras = 15,
            //!ApiIdData
            removeCamera = 16,
            getCameras = 17,
            //!ApiCameraServerItemList
            getCameraHistoryItems = 18,
            //!ApiCameraServerItem
            addCameraHistoryItem = 19,

            getMediaServers = 20,
            //!ApiMediaServer
            saveMediaServer = 21,
            //!ApiIdData
            removeMediaServer = 22,

            //!ApiUser
            saveUser = 23,
            getUsers = 24,
            //!ApiIdData
            removeUser = 25,

            //!ApiBusinessRuleList
            getBusinessRules = 26,
            //!ApiBusinessRule
            saveBusinessRule = 27,
            //!ApiIdData
            removeBusinessRule = 28,
            //!ApiBusinessActionData
            broadcastBusinessAction = 29,
            //!ApiBusinessActionData
            execBusinessAction = 30,

            //!ApiResetBusinessRuleData
            resetBusinessRules = 31,

            //!ApiLayout
            saveLayouts = 32,
            //!ApiLayoutList
            saveLayout = 33,
            //!ApiLayoutList
            getLayouts = 34,
            //!ApiIdData
            removeLayout = 35,
            
            //!ApiVideowall
            saveVideowall = 36,
            getVideowalls = 37,
            //!ApiIdData
            removeVideowall = 38,

            //!ApiVideoWallControlMessage
            videowallControl = 39,

            //!
            listDirectory = 40,
            //!ApiStoredFileData
            getStoredFile = 41,
            //!ApiStoredFileData
            addStoredFile = 42,
            //!ApiStoredFileData
            updateStoredFile = 43,
            //!QString
            removeStoredFile = 44,

            //!ApiLicenseList
            addLicenses = 45,
            //!ApiLicense
            addLicense = 46,
            //!ApiLicenseList
            getLicenses = 47,

            // ApiEmailSettingsData
            testEmailSettings = 48,

            // ApiEmailData
            sendEmail = 49,

            //!ApiResourceParamList
            getSettings = 50,
            //!ApiResourceParamList
            saveSettings = 51,

            getCurrentTime = 52,

            tranSyncRequest = 53,
            tranSyncResponse = 54,
            
            peerAliveInfo = 55,
            
            //!ApiLockInfo
            lockRequest = 56,
            lockResponse = 57,
            unlockRequest = 58,

            //!ApiUpdateUploadData
            uploadUpdate = 59,
            //!ApiUpdateUploadResponceData
            uploadUpdateResponce = 60,
            //!ApiUpdateInstallData
            installUpdate = 61,

            //!ApiCameraBookmarkTagDataList
            addCameraBookmarkTags = 62,
            getCameraBookmarkTags = 63,
            removeCameraBookmarkTags = 64,

            //ApiHelpGroupDataList
            getHelp = 65,

            //ApiVideowallInstanceStatusData
            updateVideowallInstanceStatus = 66,
            
            //ApiRuntimeData
            runtimeInfoChanged = 67
        };
        QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Value)

        QString toString( Value val );
        bool isSystem( Value val );
    }

    class QnAbstractTransaction
    {
    public:
		QnAbstractTransaction(): command(ApiCommand::NotDefined), persistent(false), timestamp(0), localTransaction(false) {}
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
        
        static QAtomicInt m_sequence;
        bool localTransaction; // do not propagate transactions to other server peers
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
    void serialize(const QnTransaction<T> &transaction, QnOutputBinaryStream<Output> *stream)
    {
        QnBinary::serialize(static_cast<const QnAbstractTransaction &>(transaction), stream);
        QnBinary::serialize(transaction.params, stream);
    }

    template <class T, class Input>
    bool deserialize(QnInputBinaryStream<Input>* stream, QnTransaction<T> *transaction)
    {
        return 
            QnBinary::deserialize(stream, static_cast<QnAbstractTransaction *>(transaction)) &&
            QnBinary::deserialize(stream, &transaction->params);
    }

    int generateRequestID();
} // namespace ec2

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ec2::ApiCommand::Value), (numeric))

#ifndef QN_NO_QT
Q_DECLARE_METATYPE(ec2::QnAbstractTransaction)
#endif

#endif  //EC2_TRANSACTION_H
