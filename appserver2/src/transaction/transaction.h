
#ifndef EC2_TRANSACTION_H
#define EC2_TRANSACTION_H

#include <QString>
#include <vector>
#include "nx_ec/binary_serialization_helper.h"
#include "nx_ec/ec_api.h"


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

            getCurrentTime
        };

        QString toString( Value val );
    }

    class QnAbstractTransaction
    {
    public:
		QnAbstractTransaction(): command(ApiCommand::NotDefined), persistent(false) {}

        //void createNewID();
        void createNewID(ApiCommand::Value command, bool persistent);
        
        static void setStartNumber(const qint64& value);

        template <class T2>
        bool deserialize(ApiCommand::Value command, InputBinaryStream<T2>* stream) 
        {
            this->command = command;
            if (!QnBinary::deserialize(id, stream))
                return false;
            if (!QnBinary::deserialize(persistent, stream))
                return false;
            return true;
        }

        struct ID
        {
            ID(): tranID(0) {}
            QUuid peerGUID;
            qint32 tranID;
        };

        ApiCommand::Value command;
        ID id;
        bool persistent;
        
        QUuid originGuid; // this field doesn't serializable and uses local only. 
        static QAtomicInt m_localSequence;
    };

    template <class T>
    class QnTransaction: public QnAbstractTransaction
    {
    public:
        QnTransaction() {}
        
        T params;

        template <class T2>
        void serialize(OutputBinaryStream<T2>* stream) const {
            QnBinary::serialize( *(QnAbstractTransaction*)this, stream);
            QnBinary::serialize(params, stream);
        }

        template <class T2>
        bool deserialize(ApiCommand::Value command, InputBinaryStream<T2>* stream) 
        {
            return QnAbstractTransaction::deserialize(command, stream) &&
                QnBinary::deserialize(params, stream);
                //params.deserialize(stream);
        }
    };

    int generateRequestID();
}

QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction::ID, (peerGUID) (tranID) )
QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction, (command) (id) (persistent))

//QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS( QnTransaction, ... )

/*
template<class T>
void serialize( Transaction, BinaryStream<T> );

template<class T>
void deserialize( BinaryStream<T> , Transaction * );

void serialize( Transaction, QJsonValue * );
void deserialize( const QJsonValue &, Transaction * );
*/

#endif  //EC2_TRANSACTION_H
