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
#include "nx_ec/data/ec2_resource_data.h"
#include "nx_ec/data/ec2_layout_data.h"
#include "nx_ec/data/ec2_videowall_data.h"
#include "nx_ec/data/camera_server_item_data.h"
#include "nx_ec/data/ec2_stored_file_data.h"
#include "nx_ec/data/ec2_full_data.h"
#include "nx_ec/data/ec2_license.h"
#include "utils/db/db_helper.h"
#include <QSet>

namespace ec2
{
    static const char ADD_HASH_DATA[] = "$$_HASH_$$";

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

        ErrorCode saveTransaction(const QnTransaction<ApiFullInfo>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiBusinessActionData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiVideowallControlMessage>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        qint64 getRelativeTime() const;
        void init();

    private:
        bool contains(const QnAbstractTransaction& tran, const QUuid& hash);
        QUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray());

        QUuid transactionHash(const QnTransaction<ApiCamera>& tran)              { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiMediaServer>& tran)         { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiUser>& tran)                { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiLayout>& tran)              { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiVideowall>& tran)           { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiBusinessRule>& tran)        { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiIdData>& tran)                  { return makeHash(tran.params.id.toRfc4122(), "ApiIdData"); }
        QUuid transactionHash(const QnTransaction<ApiSetResourceDisabledData>& tran) { return makeHash(tran.params.id.toRfc4122(), "disabled"); }
        QUuid transactionHash(const QnTransaction<ApiCameraServerItem>&)         { return QUuid::createUuid() ; }
        QUuid transactionHash(const QnTransaction<ApiSetResourceStatusData>& tran)   { return makeHash(tran.params.id.toRfc4122(), "status"); }
        QUuid transactionHash(const QnTransaction<ApiPanicModeData>&)                { return makeHash("panic_mode", ADD_HASH_DATA) ; }
        QUuid transactionHash(const QnTransaction<ApiResourceParams>& tran)          { return makeHash(tran.params.id.toRfc4122(), "res_params") ; }
        QUuid transactionHash(const QnTransaction<ApiParamList>& /*tran*/)               { return makeHash("settings", ADD_HASH_DATA) ; }
        QUuid transactionHash(const QnTransaction<ApiStoredFileData>& tran)          { return makeHash(tran.params.path.toUtf8()); }
        QUuid transactionHash(const QnTransaction<ApiStoredFilePath>& tran)          { return makeHash(tran.params.toUtf8()); }
        QUuid transactionHash(const QnTransaction<ApiResource>& tran)            { return makeHash(tran.params.id.toRfc4122(), "resource"); }
        QUuid transactionHash(const QnTransaction<ApiLicense>& tran)                 { return makeHash(tran.params.key, "ApiLicense"); }    //TODO
        QUuid transactionHash(const QnTransaction<ApiResetBusinessRuleData>& /*tran*/)   { return makeHash("reset_brule", ADD_HASH_DATA); }
        
        QUuid transactionHash(const QnTransaction<ApiFullInfo>& )                { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiCameraList>& )          { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiLayoutList>& )          { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiVideowallList>& )       { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiLicenseList>&)              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiBusinessActionData>& )      { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiEmailSettingsData>& )       { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiEmailData>& )               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiVideowallControlMessage>& ) { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }

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
