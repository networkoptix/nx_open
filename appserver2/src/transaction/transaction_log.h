#ifndef __TRANSACTION_LOG_H_
#define __TRANSACTION_LOG_H_

#include <QSet>
#include <QSql>
#include "transaction.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/data/ec2_business_rule_data.h"
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/mserver_data.h"
#include "nx_ec/data/ec2_user_data.h"
#include "nx_ec/data/ec2_layout_data.h"
#include "nx_ec/data/camera_server_item_data.h"
#include "nx_ec/data/ec2_stored_file_data.h"
#include "nx_ec/data/ec2_full_data.h"
#include "nx_ec/data/ec2_license.h"
#include "utils/db/db_helper.h"
#include <QSet>

namespace ec2
{
    class QnDbManager;

    typedef QMap<QUuid, qint32> QnTranState;

    class QnTransactionLog
    {
    public:


        QnTransactionLog(QnDbManager* db);

        static QnTransactionLog* instance();
        void initStaticInstance(QnTransactionLog* value);

        ErrorCode getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result);
        QnTranState getTransactionsState();
        
        template <class T>
        bool contains(const QnTransaction<T>& tran) { return contains(tran, transactionHash(tran)); }

        template <class T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran) {
            return saveToDB(tran, transactionHash(tran), serializedTran);
        }

        ErrorCode saveTransaction(const QnTransaction<ApiLayoutDataList>& multiTran, const QByteArray& /*serializedTran*/) {
            return saveMultiTransaction<ApiLayoutDataList, ApiLayoutData>(multiTran);
        }

        ErrorCode saveTransaction(const QnTransaction<ApiCameraDataList>& multiTran, const QByteArray& /*serializedTran*/) {
            return saveMultiTransaction<ApiCameraDataList, ApiCameraData>(multiTran);
        }

        ErrorCode saveTransaction(const QnTransaction<ApiLicenseList>& multiTran, const QByteArray& /*serializedTran*/) {
            return saveMultiTransaction<ApiLicenseList, ApiLicense>(multiTran);
        }

        ErrorCode saveTransaction(const QnTransaction<ApiFullData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode saveTransaction(const QnTransaction<ApiBusinessActionData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        qint64 getRelativeTime() const;

    private:
        bool contains(const QnAbstractTransaction& tran, const QUuid& hash);
        QUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray());

        QUuid transactionHash(const QnTransaction<ApiCameraData>& tran)              { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiMediaServerData>& tran)         { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiUserData>& tran)                { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiLayoutData>& tran)              { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiBusinessRuleData>& tran)        { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiIdData>& tran)                  { return makeHash(tran.params.id.toRfc4122(), "ApiIdData"); }
        QUuid transactionHash(const QnTransaction<ApiSetResourceDisabledData>& tran) { return makeHash(tran.params.id.toRfc4122(), "disabled"); }
        QUuid transactionHash(const QnTransaction<ApiCameraServerItemData>&)         { return QUuid::createUuid() ; }
        QUuid transactionHash(const QnTransaction<ApiSetResourceStatusData>& tran)   { return makeHash(tran.params.id.toRfc4122(), "status"); }
        QUuid transactionHash(const QnTransaction<ApiPanicModeData>&)                { return makeHash("panic_mode") ; }
        QUuid transactionHash(const QnTransaction<ApiResourceParams>& tran)          { return makeHash(tran.params.id.toRfc4122(), "res_params") ; }
        QUuid transactionHash(const QnTransaction<ApiStoredFileData>& tran)          { return makeHash(tran.params.path.toUtf8()); }
        QUuid transactionHash(const QnTransaction<ApiStoredFilePath>& tran)          { return makeHash(tran.params.toUtf8()); }
        QUuid transactionHash(const QnTransaction<ApiResourceData>& tran)            { return makeHash(tran.params.id.toRfc4122(), "resource"); }
        QUuid transactionHash(const QnTransaction<ApiLicense>& tran)                 { return makeHash(tran.params.key, "ApiLicense"); }    //TODO
        QUuid transactionHash(const QnTransaction<ApiResetBusinessRuleData>& /*tran*/)   { return makeHash("reset_brule"); }
        
        QUuid transactionHash(const QnTransaction<ApiFullData>& )                { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiCameraDataList>& )          { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiLayoutDataList>& )          { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiLicenseList>&)              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiBusinessActionData>& )      { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }

        template <class T, class T2>
        ErrorCode saveMultiTransaction(const QnTransaction<T>& multiTran)
        {
            foreach(const T2& data, multiTran.params.data)
            {
                QnTransaction<T2> tran(ApiCommand::saveLayout, true);
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
        ErrorCode saveToDB(const QnAbstractTransaction& tranID, const QUuid& hash, const QByteArray& data);
    private:
        QnDbManager* m_dbManager;
        QnTranState m_state;
        //QMap<QUuid, QnAbstractTransaction> m_updateHistory;
        QMap<QUuid, qint64> m_updateHistory;
        
        mutable QMutex m_timeMutex;
        QElapsedTimer m_relativeTimer;
        qint64 m_relativeOffset;
    };
};

#define transactionLog QnTransactionLog::instance()

#endif // __TRANSACTION_LOG_H_
