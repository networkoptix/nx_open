#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <vector>

#include <QString>

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
		    NotDefined,

            testConnection,
            connect,

            clientInstanceId,

            //!ApiResourceTypeList
            getResourceTypes,
            //!ApiResource
            getResource,
            //!ApiSetResourceStatusData
            setResourceStatus,
            //!ApiSetResourceDisabledData
            setResourceDisabled,
            //!ApiResourceParams
            setResourceParams,
            getResourceParams,
            //!ApiResource
            saveResource,
            removeResource,
            //!ApiPanicModeData
            setPanicMode,
            //!ApiFullInfo,
            getAllDataList,
            
            //!ApiCamera
            saveCamera,
            //!ApiCameraList
            saveCameras,
            //!ApiIdData
            removeCamera,
            getCameras,
            //!ApiCameraServerItemList
            getCameraHistoryList,
            //!ApiCameraServerItem
            addCameraHistoryItem,

            getMediaServerList,
            //!ApiMediaServer
            saveMediaServer,
            //!ApiIdData
            removeMediaServer,

            //!ApiUser
            saveUser,
            getUserList,
            //!ApiIdData
            removeUser,

            //!ApiBusinessRuleList
            getBusinessRuleList,
            //!ApiBusinessRule
            saveBusinessRule,
            //!ApiIdData
            removeBusinessRule,
            //!ApiBusinessActionData
            broadcastBusinessAction,

            //!ApiResetBusinessRuleData
            resetBusinessRules,

            //!ApiLayout
            saveLayouts,
            //!ApiLayoutList
            saveLayout,
            //!ApiLayoutList
            getLayoutList,
            //!ApiIdData
            removeLayout,
            
            //!ApiVideowall
            saveVideowall,
            getVideowallList,
            //!ApiIdData
            removeVideowall,

            //!ApiVideoWallControlMessage
            videowallControl,

            //!
            listDirectory,
            //!ApiStoredFileData
            getStoredFile,
            //!ApiStoredFileData
            addStoredFile,
            //!ApiStoredFileData
            updateStoredFile,
            //!QString
            removeStoredFile,

            //!ApiLicenseList
            addLicenses,
            //!ApiLicense
            addLicense,
            //!ApiLicenseList
            getLicenses,

            // ApiEmailSettingsData
            testEmailSettings,

            // ApiEmailData
            sendEmail,

            //!ApiResourceParamList
            getSettings,
            //!ApiResourceParamList
            saveSettings,

            getCurrentTime,

            tranSyncRequest,
            tranSyncResponse,
            
            serverAliveInfo,
            
            //!ApiLockInfo
            lockRequest,
            lockResponse,
            unlockRequest,
        };

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
            QUuid peerGUID;
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
}

//QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction::ID, (peerGUID) (sequence) )
//QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction, (command) (id) (persistent) (timestamp))
QN_FUSION_DECLARE_FUNCTIONS(ec2::QnAbstractTransaction::ID, (binary))
QN_FUSION_DECLARE_FUNCTIONS(ec2::QnAbstractTransaction, (binary))

namespace ec2
{
    template <class T>
    class QnTransaction: public QnAbstractTransaction
    {
    public:
        QnTransaction() {}
        QnTransaction(const QnAbstractTransaction& abstractTran): QnAbstractTransaction(abstractTran) {}
        QnTransaction(ApiCommand::Value command, bool persistent): QnAbstractTransaction(command, persistent) {}
        
        T params;
    };

    template <class T, class Output>
    void serialize(const QnTransaction<T> &transaction, QnOutputBinaryStream<Output> *stream)
    {
        QnBinary::serialize(static_cast<const QnAbstractTransaction &>(transaction), stream);
        QnBinary::serialize(transaction.params, stream);
    }

    template <class T, class Input>
    bool deserialize(QnOutputBinaryStream<Input>* stream, QnTransaction<T> *transaction)
    {
        return 
            QnBinary::deserialize(stream, static_cast<QnAbstractTransaction *>(transaction)) &&
            QnBinary::deserialize(stream, &transaction->params, stream);
    }

    int generateRequestID();
}

#endif  //EC2_TRANSACTION_H
