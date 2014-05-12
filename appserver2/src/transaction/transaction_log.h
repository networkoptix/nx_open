#ifndef __TRANSACTION_LOG_H_
#define __TRANSACTION_LOG_H_

#include <QtCore/QSet>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMutex>

#include "transaction.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_user_data.h"
#include "nx_ec/data/api_resource_data.h"
#include "nx_ec/data/api_layout_data.h"
#include "nx_ec/data/api_videowall_data.h"
#include "nx_ec/data/api_camera_server_item_data.h"
#include "nx_ec/data/api_stored_file_data.h"
#include "nx_ec/data/api_full_info_data.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_update_data.h"
#include "utils/db/db_helper.h"

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

        ErrorCode saveTransaction(const QnTransaction<ApiFullInfoData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiBusinessActionData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiVideowallControlMessageData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiUpdateUploadData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        qint64 getRelativeTime() const;
        void init();

    private:
        bool contains(const QnAbstractTransaction& tran, const QUuid& hash);
        QUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray());
        QUuid makeHash(const ApiCommand::Value command, const ApiCameraBookmarkTagDataList& data);

        QUuid transactionHash(const QnTransaction<ApiCameraData>& tran)                 { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiMediaServerData>& tran)            { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiUserData>& tran)                   { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiLayoutData>& tran)                 { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiVideowallData>& tran)              { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiBusinessRuleData>& tran)           { return tran.params.id; }
        QUuid transactionHash(const QnTransaction<ApiIdData>& tran)                     { return makeHash(tran.params.id.toRfc4122(), "ApiIdData"); } // TODO: #Elric allocation on every call => bad.
        QUuid transactionHash(const QnTransaction<ApiSetResourceDisabledData>& tran)    { return makeHash(tran.params.id.toRfc4122(), "disabled"); }
        QUuid transactionHash(const QnTransaction<ApiCameraServerItemData>&)            { return QUuid::createUuid() ; }
        QUuid transactionHash(const QnTransaction<ApiSetResourceStatusData>& tran)      { return makeHash(tran.params.id.toRfc4122(), "status"); }
        QUuid transactionHash(const QnTransaction<ApiPanicModeData>&)                   { return makeHash("panic_mode", ADD_HASH_DATA) ; }
        QUuid transactionHash(const QnTransaction<ApiResourceParamsData>& tran)         { return makeHash(tran.params.id.toRfc4122(), "res_params") ; }
        QUuid transactionHash(const QnTransaction<ApiResourceParamDataList>& /*tran*/)  { return makeHash("settings", ADD_HASH_DATA) ; }
        QUuid transactionHash(const QnTransaction<ApiStoredFileData>& tran)             { return makeHash(tran.params.path.toUtf8()); }
        QUuid transactionHash(const QnTransaction<ApiStoredFilePath>& tran)             { return makeHash(tran.params.toUtf8()); }
        QUuid transactionHash(const QnTransaction<ApiResourceData>& tran)               { return makeHash(tran.params.id.toRfc4122(), "resource"); }
        QUuid transactionHash(const QnTransaction<ApiLicenseData>& tran)                { return makeHash(tran.params.key, "ApiLicense"); }    //TODO
        QUuid transactionHash(const QnTransaction<ApiResetBusinessRuleData>& /*tran*/)  { return makeHash("reset_brule", ADD_HASH_DATA); }
        QUuid transactionHash(const QnTransaction<ApiUpdateUploadResponceData>& tran)   { return makeHash(tran.params.id.toRfc4122(), tran.params.updateId.toUtf8()); }
        QUuid transactionHash(const QnTransaction<ApiCameraBookmarkTagDataList> &tran)  { return makeHash(tran.command, tran.params); }   //TODO: #Elric ec2 make sure it is the correct way
        
        QUuid transactionHash(const QnTransaction<ApiFullInfoData>& )                   { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiCameraDataList>& )                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiLayoutDataList>& )                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiVideowallDataList>& )              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiLicenseDataList>&)                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiBusinessActionData>& )             { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiEmailSettingsData>& )              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiEmailData>& )                      { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiVideowallControlMessageData>& )    { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTransaction<ApiUpdateUploadData>& )               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }

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
