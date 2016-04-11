#ifndef __TRANSACTION_LOG_H_
#define __TRANSACTION_LOG_H_

#include <QtCore/QSet>
#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>

#include "transaction.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_camera_attributes_data.h"
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_user_data.h"
#include "nx_ec/data/api_tran_state_data.h"
#include "nx_ec/data/api_layout_data.h"
#include "nx_ec/data/api_videowall_data.h"
#include "nx_ec/data/api_camera_history_data.h"
#include "nx_ec/data/api_stored_file_data.h"
#include "nx_ec/data/api_full_info_data.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_update_data.h"
#include "nx_ec/data/api_discovery_data.h"
#include "nx_ec/data/api_routing_data.h"
#include "nx_ec/data/api_system_name_data.h"
#include "nx_ec/data/api_runtime_data.h"
#include "nx_ec/data/api_license_overflow_data.h"
#include "nx_ec/data/api_peer_system_time_data.h"
#include "nx_ec/data/api_webpage_data.h"
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
        virtual ~QnTransactionLog();

        static QnTransactionLog* instance();

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

        ErrorCode saveTransaction(const QnTransaction<ApiStorageDataList>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiFullInfoData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiLicenseOverflowData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiBusinessActionData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiVideowallControlMessageData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiUpdateInstallData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiUpdateUploadData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiDiscoveredServerData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiDiscoveredServerDataList>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiRuntimeData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiDiscoverPeerData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiSystemNameData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiSyncRequestData>& , const QByteArray&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        qint64 getTimeStamp();
        bool init();

        int getLatestSequence(const QnTranStateKey& key) const;
        QnUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray()) const;
        QnUuid makeHash(const QByteArray &extraData, const ApiDiscoveryData &data) const;

         /**
         *  Semantics of the transactionHash() function is following:
         *  if transaction A follows transaction B and overrides it,
         *  their transactionHash() result MUST be the same. Otherwise, transactionHash() result must differ.
         *  Obviously, transactionHash() is not needed for the non-persistent transaction.
         */

        QnUuid transactionHash(const ApiCameraData& params) const                 { return params.id; }
        QnUuid transactionHash(const ApiCameraAttributesData& params) const       { return makeHash(params.cameraID.toRfc4122(), "camera_attributes"); }
        QnUuid transactionHash(const ApiMediaServerData& params) const            { return params.id; }
        QnUuid transactionHash(const ApiStorageData& params) const                { return params.id; }
		QnUuid transactionHash(const ApiMediaServerUserAttributesData& params) const    { return makeHash(params.serverID.toRfc4122(), "server_attributes"); }
        QnUuid transactionHash(const ApiUserData& params) const                   { return params.id; }
        QnUuid transactionHash(const ApiAccessRightsData& params) const           { return params.userId; }
        QnUuid transactionHash(const ApiLayoutData& params) const                 { return params.id; }
        QnUuid transactionHash(const ApiVideowallData& params) const              { return params.id; }
        QnUuid transactionHash(const ApiWebPageData &params) const                { return params.id; }
        QnUuid transactionHash(const ApiBusinessRuleData& params) const           { return params.id; }
        QnUuid transactionHash(const ApiIdData& params) const                     { return params.id; }
        QnUuid transactionHash(const ApiServerFootageData& params) const       { return makeHash(params.serverGuid.toRfc4122(), "history"); }
        QnUuid transactionHash(const ApiResourceStatusData& params) const      { return makeHash(params.id.toRfc4122(), "status"); }
        QnUuid transactionHash(const ApiResourceParamWithRefData& param) const;
        QnUuid transactionHash(const ApiStoredFileData& params) const             { return makeHash(params.path.toUtf8()); }
        QnUuid transactionHash(const ApiStoredFilePath& params) const             { return makeHash(params.path.toUtf8()); }
        QnUuid transactionHash(const ApiLicenseData& params) const                { return makeHash(params.key, "ApiLicense"); }    //TODO
        QnUuid transactionHash(const ApiResetBusinessRuleData& /*tran*/) const    { return makeHash("reset_brule", ADD_HASH_DATA); }
        QnUuid transactionHash(const ApiDiscoveryData &params) const              { return makeHash("discovery_data", params); }

        QnUuid transactionHash(const ApiIdDataList& /*tran*/) const             { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiResourceParamDataList& /*tran*/) const  { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiResourceParamWithRefDataList& /*tran*/) const  { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiFullInfoData& ) const                   { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiCameraDataList& ) const                 { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiStorageDataList& ) const                { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiCameraAttributesDataList& ) const       { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiMediaServerUserAttributesDataList& ) const       { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiLayoutDataList& ) const                 { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiVideowallDataList& ) const              { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiWebPageDataList& ) const                { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiLicenseDataList&) const                 { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiBusinessActionData& ) const             { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiEmailSettingsData& ) const              { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiEmailData& ) const                      { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiVideowallControlMessageData& ) const    { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }

        QnUuid transactionHash(const ApiUpdateInstallData& ) const              { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiUpdateUploadData& ) const               { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiUpdateUploadResponceData& ) const       { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiDiscoveredServerData& ) const           { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiDiscoveredServerDataList& ) const       { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiDiscoverPeerData& ) const               { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiSystemNameData& ) const                 { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiDiscoveryDataList& ) const              { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }

        QnUuid transactionHash(const ApiLockData& ) const                       { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiPeerAliveData& ) const                  { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiSyncRequestData& ) const                { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const QnTranStateResponse& ) const               { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiRuntimeData& ) const                    { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiPeerSystemTimeData& ) const             { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiPeerSystemTimeDataList& ) const         { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiDatabaseDumpData& ) const               { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiUpdateSequenceData& ) const             { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiTranSyncDoneData& ) const               { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiLicenseOverflowData& ) const            { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }
        QnUuid transactionHash(const ApiClientInfoData& params) const           { return makeHash(params.id.toRfc4122()); }
        QnUuid transactionHash(const ApiReverseConnectionData& ) const          { NX_ASSERT(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QnUuid(); }

        ErrorCode updateSequence(const ApiUpdateSequenceData& data);
        void fillPersistentInfo(QnAbstractTransaction& tran);

        void beginTran();
        void commit();
        void rollback();

        qint64 getTransactionLogTime() const;
        void setTransactionLogTime(qint64 value);
    private:
        friend class QnDbManager;

        template <class T>
        ContainsReason contains(const QnTransaction<T>& tran) { return contains(tran, transactionHash(tran.params)); }
        ContainsReason contains(const QnAbstractTransaction& tran, const QnUuid& hash) const;

        int currentSequenceNoLock() const;

        ErrorCode saveToDB(const QnAbstractTransaction& tranID, const QnUuid& hash, const QByteArray& data);
        ErrorCode updateSequenceNoLock(const QnUuid& peerID, const QnUuid& dbID, int sequence);
    private:
        struct UpdateHistoryData
        {
            UpdateHistoryData(): timestamp(0) {}
            UpdateHistoryData(const QnTranStateKey& updatedBy, const qint64& timestamp): updatedBy(updatedBy), timestamp(timestamp) {}
            QnTranStateKey updatedBy;
            qint64 timestamp;
        };
        struct CommitData
        {
            CommitData() {}
            void clear() { state.values.clear(); updateHistory.clear(); }

            QnTranState state;
            QMap<QnUuid, UpdateHistoryData> updateHistory;
        };
    private:
        QnDbManager* m_dbManager;
        QnTranState m_state;
        QMap<QnUuid, UpdateHistoryData> m_updateHistory;

        mutable QnMutex m_timeMutex;
        QElapsedTimer m_relativeTimer;
        qint64 m_baseTime;
        qint64 m_lastTimestamp;
        CommitData m_commitData;
    };
};

#define transactionLog QnTransactionLog::instance()

#endif // __TRANSACTION_LOG_H_
