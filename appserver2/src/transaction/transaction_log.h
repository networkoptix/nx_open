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
        bool contains(const QnTransaction<T>& tran) { return contains(tran, transactionHash(tran.params)); }

        template <class T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran) {
            return saveToDB(tran, transactionHash(tran.params), serializedTran);
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

        ErrorCode saveTransaction(const QnTransaction<ApiModuleData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        qint64 getRelativeTime() const;
        void init();

    private:
        bool contains(const QnAbstractTransaction& tran, const QUuid& hash) const;
        QUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray()) const;
        QUuid makeHash(const QString& extraData, const ApiCameraBookmarkTagDataList& data);

        QUuid transactionHash(const ApiCameraData& params) const                 { return params.id; }
        QUuid transactionHash(const ApiMediaServerData& params) const            { return params.id; }
        QUuid transactionHash(const ApiUserData& params) const                   { return params.id; }
        QUuid transactionHash(const ApiLayoutData& params) const                 { return params.id; }
        QUuid transactionHash(const ApiVideowallData& params) const              { return params.id; }
        QUuid transactionHash(const ApiBusinessRuleData& params) const           { return params.id; }
        QUuid transactionHash(const ApiIdData& params) const                     { return makeHash(params.id.toRfc4122(), "ApiIdData"); } // TODO: #Elric allocation on every call => bad.
        //QUuid transactionHash(const ApiSetResourceDisabledData& params) const    { return makeHash(params.id.toRfc4122(), "disabled"); }
        QUuid transactionHash(const ApiCameraServerItemData&) const              { return QUuid::createUuid() ; }
        QUuid transactionHash(const ApiSetResourceStatusData& params) const      { return makeHash(params.id.toRfc4122(), "status"); }
        QUuid transactionHash(const ApiPanicModeData&) const                     { return makeHash("panic_mode", ADD_HASH_DATA) ; }
        QUuid transactionHash(const ApiResourceParamsData& params) const         { return makeHash(params.id.toRfc4122(), "res_params") ; }
        QUuid transactionHash(const ApiResourceParamDataList& /*tran*/) const    { return makeHash("settings", ADD_HASH_DATA) ; }
        QUuid transactionHash(const ApiStoredFileData& params) const             { return makeHash(params.path.toUtf8()); }
        QUuid transactionHash(const ApiStoredFilePath& params) const             { return makeHash(params.toUtf8()); }
        QUuid transactionHash(const ApiResourceData& params) const               { return makeHash(params.id.toRfc4122(), "resource"); }
        QUuid transactionHash(const ApiLicenseData& params) const                { return makeHash(params.key, "ApiLicense"); }    //TODO
        QUuid transactionHash(const ApiResetBusinessRuleData& /*tran*/) const    { return makeHash("reset_brule", ADD_HASH_DATA); }
        QUuid transactionHash(const ApiCameraBookmarkTagDataList& params)        { return makeHash("add_bookmark_tags", params); }
        
        QUuid transactionHash(const ApiFullInfoData& ) const                   { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiCameraDataList& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiLayoutDataList& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiVideowallDataList& ) const              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiLicenseDataList&) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiBusinessActionData& ) const             { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiEmailSettingsData& ) const              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiEmailData& ) const                      { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiVideowallControlMessageData& ) const    { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiUpdateUploadData& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiUpdateUploadResponceData& ) const       { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiModuleData& ) const                     { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }

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
