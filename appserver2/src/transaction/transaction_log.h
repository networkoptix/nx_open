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
#include "nx_ec/data/api_tran_state_data.h"
#include "nx_ec/data/api_layout_data.h"
#include "nx_ec/data/api_videowall_data.h"
#include "nx_ec/data/api_camera_server_item_data.h"
#include "nx_ec/data/api_stored_file_data.h"
#include "nx_ec/data/api_full_info_data.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_update_data.h"
#include "nx_ec/data/api_module_data.h"
#include "nx_ec/data/api_camera_bookmark_data.h"
#include "nx_ec/data/api_routing_data.h"
#include "nx_ec/data/api_system_name_data.h"
#include "nx_ec/data/api_runtime_data.h"
#include "nx_ec/data/api_license_overflow_data.h"
#include "nx_ec/data/api_peer_system_time_data.h"
#include "utils/db/db_helper.h"
#include "binary_transaction_serializer.h"
#include "ubjson_transaction_serializer.h"

namespace ec2
{
    static const char ADD_HASH_DATA[] = "$$_HASH_$$";

    class QnDbManager;


    class QnTransactionLog
    {
    public:

        enum ContainsReason {
            Reason_None,
            Reason_Sequence,
            Reason_Timestamp
        };

        QnTransactionLog(QnDbManager* db);

        static QnTransactionLog* instance();
        void initStaticInstance(QnTransactionLog* value);

        ErrorCode getTransactionsAfter(const QnTranState& state, QList<QByteArray>& result);
        QnTranState getTransactionsState();
        
        bool contains(const QnTranState& state) const;

        template <class T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran) 
        {
            QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
            return saveToDB(tran, transactionHash(tran.params), serializedTran);
        }

        template <class T>
        ErrorCode saveTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran) {
            return saveToDB(tran, transactionHash(tran.params), serializedTran);
        }

        ErrorCode saveTransaction(const QnTransaction<ApiFullInfoData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiLicenseOverflowData>& , const QByteArray&) {
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

        ErrorCode saveTransaction(const QnTransaction<ApiUpdateInstallData>& , const QByteArray&) {
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

        ErrorCode saveTransaction(const QnTransaction<ApiRuntimeData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiModuleDataList>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiDiscoverPeerData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiSystemNameData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiSyncRequestData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        qint64 getTimeStamp();
        void init();

        int getLatestSequence(const QnTranStateKey& key) const;
        QnUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray()) const;
        QnUuid makeHash(const QString& extraData, const ApiCameraBookmarkTagDataList& data) const;
        QnUuid makeHash(const QString &extraData, const ApiDiscoveryData &data) const;

         /**
         *  Semantics of the transactionHash() function is following:
         *  if transaction A follows transaction B and overrides it,
         *  their transactionHash() result MUST be the same. Otherwise, transactionHash() result must differ.
         *  Obviously, transactionHash() is not needed for the non-persistent transaction.
         */

        QnUuid transactionHash(const ApiCameraData& params) const                 { return params.id; }
        QnUuid transactionHash(const ApiMediaServerData& params) const            { return params.id; }
        QnUuid transactionHash(const ApiUserData& params) const                   { return params.id; }
        QnUuid transactionHash(const ApiLayoutData& params) const                 { return params.id; }
        QnUuid transactionHash(const ApiVideowallData& params) const              { return params.id; }
        QnUuid transactionHash(const ApiBusinessRuleData& params) const           { return params.id; }
        QnUuid transactionHash(const ApiIdData& params) const                     { return params.id; }
        QnUuid transactionHash(const ApiCameraServerItemData& params) const       { return makeHash(params.cameraUniqueId.toUtf8(), QByteArray::number(params.timestamp)); }
        QnUuid transactionHash(const ApiSetResourceStatusData& params) const      { return makeHash(params.id.toRfc4122(), "status"); }
        QnUuid transactionHash(const ApiPanicModeData&) const                     { return makeHash("panic_mode", ADD_HASH_DATA) ; }
        QnUuid transactionHash(const ApiResourceParamsData& params) const;
        QnUuid transactionHash(const ApiResourceParamDataList& /*tran*/) const    { return makeHash("settings", ADD_HASH_DATA) ; }
        QnUuid transactionHash(const ApiStoredFileData& params) const             { return makeHash(params.path.toUtf8()); }
        QnUuid transactionHash(const ApiStoredFilePath& params) const             { return makeHash(params.path.toUtf8()); }
        QnUuid transactionHash(const ApiLicenseData& params) const                { return makeHash(params.key, "ApiLicense"); }    //TODO
        QnUuid transactionHash(const ApiResetBusinessRuleData& /*tran*/) const    { return makeHash("reset_brule", ADD_HASH_DATA); }
        QnUuid transactionHash(const ApiCameraBookmarkTagDataList& params) const  { return makeHash("add_bookmark_tags", params); } // todo: #GDM it's invalid pattern! need refactor
        QnUuid transactionHash(const ApiDiscoveryData &params) const              { return makeHash("discovery_data", params); }
        
        QnUuid transactionHash(const ApiFullInfoData& ) const                   { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiCameraDataList& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiLayoutDataList& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiVideowallDataList& ) const              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiLicenseDataList&) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiBusinessActionData& ) const             { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiEmailSettingsData& ) const              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiEmailData& ) const                      { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiVideowallControlMessageData& ) const    { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }

        QnUuid transactionHash(const ApiUpdateInstallData& ) const              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiUpdateUploadData& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiUpdateUploadResponceData& ) const       { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiModuleData& ) const                     { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiModuleDataList& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiDiscoverPeerData& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiSystemNameData& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }

        QnUuid transactionHash(const ApiLockData& ) const                       { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiPeerAliveData& ) const                  { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiSyncRequestData& ) const                { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const QnTranStateResponse& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiRuntimeData& ) const                    { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiPeerSystemTimeData& ) const             { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiPeerSystemTimeDataList& ) const         { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiDatabaseDumpData& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiResourceData& ) const                   { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiUpdateSequenceData& ) const             { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiTranSyncDoneData& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiLicenseOverflowData& ) const            { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }

        ErrorCode updateSequence(const ApiUpdateSequenceData& data);
        void fillPersistentInfo(QnAbstractTransaction& tran);
    private:
        friend class QnDbManager;

        template <class T>
        ContainsReason contains(const QnTransaction<T>& tran) { return contains(tran, transactionHash(tran.params)); }
        ContainsReason contains(const QnAbstractTransaction& tran, const QnUuid& hash) const;

        int currentSequenceNoLock() const;

        ErrorCode saveToDB(const QnAbstractTransaction& tranID, const QnUuid& hash, const QByteArray& data);
        ErrorCode updateSequenceNoLock(const QnUuid& peerID, const QnUuid& dbID, int sequence);
    private:
        QnDbManager* m_dbManager;
        QnTranState m_state;
        struct UpdateHistoryData
        {
            UpdateHistoryData(): timestamp(0) {}
            UpdateHistoryData(const QnTranStateKey& updatedBy, const qint64& timestamp): updatedBy(updatedBy), timestamp(timestamp) {}
            QnTranStateKey updatedBy;
            qint64 timestamp;
        };

        QMap<QnUuid, UpdateHistoryData> m_updateHistory;
        
        mutable QMutex m_timeMutex;
        QElapsedTimer m_relativeTimer;
        qint64 m_currentTime;
        qint64 m_lastTimestamp;
        //QMap<QnTranStateKey, QnAbstractTransaction::PersistentInfo> m_fillerInfo;
    };
};

#define transactionLog QnTransactionLog::instance()

#endif // __TRANSACTION_LOG_H_
