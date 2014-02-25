#ifndef __TRANSACTION_LOG_H_
#define __TRANSACTION_LOG_H_

#include <QSet>
#include <QSql>
#include "nx_ec/ec_api.h"
#include "transaction.h"
#include "nx_ec/data/ec2_business_rule_data.h"
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/mserver_data.h"
#include "nx_ec/data/ec2_user_data.h"
#include "nx_ec/data/ec2_layout_data.h"
#include "nx_ec/data/camera_server_item_data.h"
#include "nx_ec/data/ec2_stored_file_data.h"
#include "nx_ec/data/ec2_full_data.h"
#include "utils/db/db_helper.h"
#include <QSet>

namespace ec2
{
    class QnDbManager;

    class QnTransactionLog
    {
    public:

        typedef QMap<QUuid, qint32> QnTranState;

        QnTransactionLog(QnDbManager* db);

        static QnTransactionLog* instance();
        void initStaticInstance(QnTransactionLog* value);

        ErrorCode getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result);
        ErrorCode getTransactionsState(QnTranState& state);
        
        QByteArray serializeState(const QnTranState& state);
        QnTransactionLog::QnTranState deserializeState(const QByteArray& buffer);

        template <class T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran) {
            return saveToDB(tran.id, transactionHash(tran), serializedTran);
        }

        ErrorCode saveTransaction(const QnTransaction<ApiLayoutDataList>& multiTran, const QByteArray& serializedTran) {
            return saveMultiTransaction<ApiLayoutDataList, ApiLayoutData>(multiTran);
        }

        ErrorCode saveTransaction(const QnTransaction<ApiCameraDataList>& multiTran, const QByteArray& serializedTran) {
            return saveMultiTransaction<ApiCameraDataList, ApiCameraData>(multiTran);
        }

        ErrorCode saveTransaction(const QnTransaction<ApiFullData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode saveTransaction(const QnTransaction<ApiBusinessActionData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

    private:
        QUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray());

        template <class T>
        QUuid transactionHash(const QnTransaction<T>& tran)
        {
            static_assert( false, "You have to add QnTransactionLog::transactionHash specification" );
        }

        QUuid transactionHash(const QnTransaction<ApiCameraData>& tran)              { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiMediaServerData>& tran)         { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiUserData>& tran)                { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiLayoutData>& tran)              { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiBusinessRuleData>& tran)        { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiIdData>& tran)                  { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiSetResourceStatusData>& tran)   { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiSetResourceDisabledData>& tran) { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiCameraServerItemData>& tran)    { return QUuid::createUuid() ; }
        QUuid transactionHash(const QnTransaction<ApiPanicModeData>& tran)           { return makeHash("panic_mode") ; }
        QUuid transactionHash(const QnTransaction<ApiResourceParams>& tran)          { return makeHash(tran.params.id.toByteArray(), "res_params") ; }
        QUuid transactionHash(const QnTransaction<ApiStoredFileData>& tran)          { return makeHash(tran.params.path.toUtf8()); }
        QUuid transactionHash(const QnTransaction<ApiStoredFilePath>& tran)          { return makeHash(tran.params.toUtf8()); }
        QUuid transactionHash(const QnTransaction<ApiResourceData>& tran)            { return makeHash(tran.params.id.toRfc4122(), "resource"); }

        template <class T, class T2>
        ErrorCode saveMultiTransaction(const QnTransaction<T>& multiTran)
        {
            foreach(const T2& data, multiTran.params.data)
            {
                QnTransaction<T2> tran;
                tran.createNewID(ApiCommand::saveLayout, true);
                tran.id.peerGUID = multiTran.id.peerGUID;
                tran.params = data;
                QByteArray serializedTran;
                OutputBinaryStream<QByteArray> stream(&serializedTran);
                tran.serialize(&stream);
                ErrorCode result = saveTransaction(tran, serializedTran);
                if (result != ErrorCode::ok) {
                    return result;
                }
            }
            return ErrorCode::ok;
        }
    private:
        ErrorCode saveToDB(const QnAbstractTransaction::ID& tranID, const QUuid& hash, const QByteArray& data);
    private:
        QnDbManager* m_dbManager;
        QSet<QUuid> m_peerList;
    };
};

#define transactionLog QnTransactionLog::instance()

#endif // __TRANSACTION_LOG_H_
