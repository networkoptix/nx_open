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
#include "nx_ec/data/api_module_data.h"
#include "nx_ec/data/api_camera_bookmark_data.h"
#include "nx_ec/data/api_routing_data.h"
#include "nx_ec/data/api_system_name_data.h"
#include "nx_ec/data/api_runtime_data.h"
#include "nx_ec/data/api_peer_system_time_data.h"
#include "utils/db/db_helper.h"
#include "binary_transaction_serializer.h"
#include "ubjson_transaction_serializer.h"

namespace ec2
{
    static const char ADD_HASH_DATA[] = "$$_HASH_$$";

    class QnDbManager;

    struct QnTranStateKey {
        QnTranStateKey() {}
        QnTranStateKey(QUuid peerID, QUuid dbID): peerID(peerID), dbID(dbID) {}
        QUuid peerID;
        QUuid dbID;

        bool operator<(const QnTranStateKey& other) const {
            if (peerID != other.peerID)
                return peerID < other.peerID;
            return dbID < other.dbID;
        }
        bool operator>(const QnTranStateKey& other) const {
            if (peerID != other.peerID)
                return peerID > other.peerID;
            return dbID > other.dbID;
        }
    };
    #define QnTranStateKey_Fields (peerID)(dbID)

    struct QnTranState {
         QMap<QnTranStateKey, qint32> values;
    };
    #define QnTranState_Fields (values)

    struct QnTranStateResponse
    {
        int result;
    };
    #define QnTranStateResponse_Fields (result)
  
    QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnTranStateKey)(QnTranState)(QnTranStateResponse), (binary)(json)(ubjson))

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

        ErrorCode saveTransaction(const QnTransaction<ApiConnectionData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiConnectionDataList>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode saveTransaction(const QnTransaction<ApiSystemNameData>& , const QByteArray&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        qint64 getTimeStamp();
        void init();

        bool contains(const QnAbstractTransaction& tran, const QUuid& hash) const;
        QUuid makeHash(const QByteArray& data1, const QByteArray& data2 = QByteArray()) const;
        QUuid makeHash(const QString& extraData, const ApiCameraBookmarkTagDataList& data) const;
        QUuid makeHash(const QString &extraData, const ApiDiscoveryDataList &data) const;

         /**
         *  Semantics of the transactionHash() function is following:
         *  if transaction A follows transaction B and overrides it,
         *  their transactionHash() result MUST be the same. Otherwise, transactionHash() result must differ.
         *  Obviously, transactionHash() is not needed for the non-persistent transaction.
         */

        QUuid transactionHash(const ApiCameraData& params) const                 { return params.id; }
        QUuid transactionHash(const ApiMediaServerData& params) const            { return params.id; }
        QUuid transactionHash(const ApiUserData& params) const                   { return params.id; }
        QUuid transactionHash(const ApiLayoutData& params) const                 { return params.id; }
        QUuid transactionHash(const ApiVideowallData& params) const              { return params.id; }
        QUuid transactionHash(const ApiBusinessRuleData& params) const           { return params.id; }
        QUuid transactionHash(const ApiIdData& params) const                     { return params.id; }
        QUuid transactionHash(const ApiCameraServerItemData&) const              { return QUuid::createUuid() ; }
        QUuid transactionHash(const ApiSetResourceStatusData& params) const      { return makeHash(params.id.toRfc4122(), "status"); }
        QUuid transactionHash(const ApiPanicModeData&) const                     { return makeHash("panic_mode", ADD_HASH_DATA) ; }
        QUuid transactionHash(const ApiResourceParamsData& params) const;
        QUuid transactionHash(const ApiResourceParamDataList& /*tran*/) const    { return makeHash("settings", ADD_HASH_DATA) ; }
        QUuid transactionHash(const ApiStoredFileData& params) const             { return makeHash(params.path.toUtf8()); }
        QUuid transactionHash(const ApiStoredFilePath& params) const             { return makeHash(params.path.toUtf8()); }
        QUuid transactionHash(const ApiLicenseData& params) const                { return makeHash(params.key, "ApiLicense"); }    //TODO
        QUuid transactionHash(const ApiResetBusinessRuleData& /*tran*/) const    { return makeHash("reset_brule", ADD_HASH_DATA); }
        QUuid transactionHash(const ApiCameraBookmarkTagDataList& params) const  { return makeHash("add_bookmark_tags", params); }
        QUuid transactionHash(const ApiDiscoveryDataList &params) const          { return makeHash("discovery_data_list", params); }
        
        QUuid transactionHash(const ApiFullInfoData& ) const                   { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiCameraDataList& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiLayoutDataList& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiVideowallDataList& ) const              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiLicenseDataList&) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiBusinessActionData& ) const             { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiEmailSettingsData& ) const              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiEmailData& ) const                      { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiVideowallControlMessageData& ) const    { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }

        QUuid transactionHash(const ApiUpdateInstallData& ) const              { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiUpdateUploadData& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiUpdateUploadResponceData& ) const       { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiModuleData& ) const                     { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiModuleDataList& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiDiscoverPeerData& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiConnectionData& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiConnectionDataList& ) const             { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiSystemNameData& ) const                 { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }

        QUuid transactionHash(const ApiLockData& ) const                       { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiPeerAliveData& ) const                  { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTranState& ) const                       { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const QnTranStateResponse& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiRuntimeData& ) const                    { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiPeerSystemTimeData& ) const             { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiDatabaseDumpData& ) const               { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }
        QUuid transactionHash(const ApiResourceData& ) const                   { Q_ASSERT_X(0, Q_FUNC_INFO, "Invalid transaction for hash!"); return QUuid(); }


    private:
        ErrorCode saveToDB(const QnAbstractTransaction& tranID, const QUuid& hash, const QByteArray& data);
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

        QMap<QUuid, UpdateHistoryData> m_updateHistory;
        
        mutable QMutex m_timeMutex;
        QElapsedTimer m_relativeTimer;
        qint64 m_currentTime;
        qint64 m_lastTimestamp;
    };
};

#define transactionLog QnTransactionLog::instance()

#endif // __TRANSACTION_LOG_H_
