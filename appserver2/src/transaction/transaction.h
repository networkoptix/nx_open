
#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <QString>
#include <vector>
#include "nx_ec/binary_serialization_helper.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/data/ec2_license.h"


namespace ec2
{
    namespace ApiCommand
    {
        enum Value
        {
		    NotDefined,

            testConnection,
            connect,

            //!ApiResourceTypeList
            getResourceTypes,
            //!ApiResourceData
            getResource,
            //!ApiSetResourceStatusData
            setResourceStatus,
            //!ApiSetResourceDisabledData
            setResourceDisabled,
            //!ApiResourceParams
            setResourceParams,
            getResourceParams,
            //!ApiResourceData
            saveResource,
            removeResource,
            //!ApiPanicModeData
            setPanicMode,
            //!ApiFullData,
            getAllDataList,
            
            //!ApiCameraData
            saveCamera,
            //!ApiCameraDataList
            saveCameras,
            //!ApiIdData
            removeCamera,
            getCameras,
            //!ApiCameraServerItemDataList
            getCameraHistoryList,
            //!ApiCameraServerItemData
            addCameraHistoryItem,

            getMediaServerList,
            //!ApiMediaServerData
            saveMediaServer,
            //!ApiIdData
            removeMediaServer,

            //!ApiUserData
            saveUser,
            getUserList,
            //!ApiIdData
            removeUser,

            //!ApiBusinessRuleDataList
            getBusinessRuleList,
            //!ApiBusinessRuleData
            saveBusinessRule,
            //!ApiIdData
            removeBusinessRule,
            //!ApiBusinessActionData
            broadcastBusinessAction,

            //!ApiResetBusinessRuleData
            resetBusinessRules,

            //!ApiLayoutData
            saveLayouts,
            //!ApiLayoutDataList
            saveLayout,
            //!ApiLayoutDataList
            getLayoutList,
            //!ApiIdData
            removeLayout,
            
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
            //!ApiLicenseList
            getLicenses,

            getCurrentTime,

            tranSyncRequest,
            tranSyncResponse,
            
            serverAliveInfo
        };

        QString toString( Value val );
    }

    class QnAbstractTransaction
    {
    public:
		QnAbstractTransaction(): command(ApiCommand::NotDefined), persistent(false), timestamp(0), localTransaction(false) {}
        QnAbstractTransaction(ApiCommand::Value command, bool persistent);
        
        void fillSequence();

        template <class T2>
        bool deserialize(InputBinaryStream<T2>* stream) 
        {
            if (!QnBinary::deserialize(command, stream))
                return false;
            if (!QnBinary::deserialize(id, stream))
                return false;
            if (!QnBinary::deserialize(persistent, stream))
                return false;
            if (!QnBinary::deserialize(timestamp, stream))
                return false;
            return true;
        }

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
        
        static QAtomicInt m_localSequence;
        bool localTransaction; // do not propagate transactions to other server peers
    };
}

QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction::ID, (peerGUID) (sequence) )
QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction, (command) (id) (persistent) (timestamp))

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

        template <class T2>
        void serialize(OutputBinaryStream<T2>* stream) const {
            QnBinary::serialize( *(QnAbstractTransaction*)this, stream);
            QnBinary::serialize(params, stream);
        }

        template <class T2>
        bool deserialize(InputBinaryStream<T2>* stream) 
        {
            return QnAbstractTransaction::deserialize(stream) &&
                QnBinary::deserialize(params, stream);
                //params.deserialize(stream);
        }
    };

    int generateRequestID();
}

#endif  //EC2_TRANSACTION_H
