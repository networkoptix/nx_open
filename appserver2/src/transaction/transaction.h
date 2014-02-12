
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
            //!ApiResourceDataList
            getResources,
            //!ApiResourceData
            getResource,
            //!ApiSetResourceStatusData
            setResourceStatus,
            //!ApiResourceParams
            setResourceParams,
            getResourceParams,
            saveResource,
            removeResource,
            //!ApiPanicModeData
            setPanicMode,
            //!ApiFullData,
            getAllDataList,
            
            //!ApiCameraData
            addCamera,
            //!ApiCameraData
		    updateCamera,
            //!ApiIdData
            removeCamera,
            getCameras,
            //!ApiCameraServerItemDataList
            getCameraHistoryList,
            //!ApiCameraServerItemData
            addCameraHistoryItem,

            getMediaServerList,
            //!ApiMediaServerData
            addMediaServer,
            //!ApiMediaServerData
            updateMediaServer,
            //!ApiIdData
            removeMediaServer,

            //!ApiUserData
            addUser,
            //!ApiUserData
            updateUser,
            getUserList,
            //!ApiIdData
            removeUser,

            //!ApiBusinessRuleDataList
            getBusinessRuleList,
            //!ApiBusinessRuleData
            addBusinessRule,
            //!ApiBusinessRuleData
            updateBusinessRule,
            //!ApiIdData
            removeBusinessRule,

            //!ApiLayoutData
            addOrUpdateLayouts,
            addLayout,  //TODO choose addOrUpdateLayouts or (addLayout and updateLayout)
            updateLayout,
            //!ApiLayoutDataList
            getLayoutList,
            //!ApiIdData
            removeLayout,
            
            //!
            listDirectory,
            //!ApiStoredFileData
            getStoredFile,
            //!ApiStoredFileData
            addOrUpdateStoredFile,
            //!QString
            removeStoredFile,

            getCurrentTime
        };

        QString toString( Value val );
    }

    struct QnUuid {
		QnUuid(): data1(0), data2(0), data3(0), data4(0) {}

        quint32 data1;
        quint16 data2;
        quint16 data3;
        qint64  data4;

    };

    class QnAbstractTransaction
    {
    public:
		QnAbstractTransaction(): command(ApiCommand::NotDefined), persistent(false) {}

        void createNewID();
        
        static void setPeerGuid(const QnUuid& value);
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
			ID(): number(0) {}

            QnUuid peerGUID;
            qint64 number;

        };

        ApiCommand::Value command;
        ID id;
        bool persistent;

    private:
        static QnUuid m_staticPeerGUID;
        static qint64 m_staticNumber;
        static QMutex m_mutex;
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
    int generateUniqueID();
}

QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnUuid, (data1) (data2) (data3) (data4) )
QN_DEFINE_STRUCT_SERIALIZATORS(ec2::QnAbstractTransaction::ID, (peerGUID) (number) )
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
