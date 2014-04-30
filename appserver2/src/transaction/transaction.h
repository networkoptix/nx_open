#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <vector>

#include <QtCore/QString>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_email_data.h"
#include "utils/serialization/binary.h"


namespace ec2
{
    namespace ApiCommand
    {
        enum Value
        {
		    NotDefined = 0,

            testConnection = 1,
            connect = 2,

            clientInstanceId = 3,

            //!ApiResourceTypeList
            getResourceTypes = 4,
            //!ApiResource
            getResource = 5,
            //!ApiSetResourceStatusData
            setResourceStatus = 6,
            //!ApiSetResourceDisabledData
            setResourceDisabled = 7,
            //!ApiResourceParams
            setResourceParams = 8,
            getResourceParams = 9,
            //!ApiResource
            saveResource = 10,
            removeResource = 11,
            //!ApiPanicModeData
            setPanicMode = 12,
            //!ApiFullInfo,
            getAllDataList = 13,
            
            //!ApiCameraData
            saveCamera = 14,
            //!ApiCameraList
            saveCameras = 15,
            //!ApiIdData
            removeCamera = 16,
            getCameras = 17,
            //!ApiCameraServerItemList
            getCameraHistoryList = 18,
            //!ApiCameraServerItem
            addCameraHistoryItem = 19,

            getMediaServerList = 20,
            //!ApiMediaServer
            saveMediaServer = 21,
            //!ApiIdData
            removeMediaServer = 22,

            //!ApiUser
            saveUser = 23,
            getUserList = 24,
            //!ApiIdData
            removeUser = 25,

            //!ApiBusinessRuleList
            getBusinessRuleList = 26,
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
            getLayoutList = 34,
            //!ApiIdData
            removeLayout = 35,
            
            //!ApiVideowall
            saveVideowall = 36,
            getVideowallList = 37,
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
            
            serverAliveInfo = 55,
            
            //!ApiLockInfo
            lockRequest = 56,
            lockResponse = 57,
            unlockRequest = 58,

            //!ApiCameraBookmarkTagDataList
            addCameraBookmarkTags = 59,
            getCameraBookmarkTags = 60,
            removeCameraBookmarkTags = 61,
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
            QUuid peerGUID; // TODO: #Elric #EC2 rename into sane case
            qint32 sequence;
        };

        ApiCommand::Value command;
        ID id;
        bool persistent;
        qint64 timestamp;
        
        static QAtomicInt m_sequence;
        bool localTransaction; // do not propagate transactions to other server peers
    };

    typedef QSet<QnId> PeerList;

    template <class T>
    class QnTransaction: public QnAbstractTransaction
    {
    public:
        QnTransaction() {}
        QnTransaction(const QnAbstractTransaction& abstractTran): QnAbstractTransaction(abstractTran) {}
        QnTransaction(ApiCommand::Value command, bool persistent): QnAbstractTransaction(command, persistent) {}
        
        T params;
    };

    QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction::ID, (binary))
    QN_FUSION_DECLARE_FUNCTIONS(QnAbstractTransaction, (binary))

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

#endif  //EC2_TRANSACTION_H
