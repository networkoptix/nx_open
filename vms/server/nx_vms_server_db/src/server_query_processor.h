#pragma once

#include <utils/common/scoped_thread_rollback.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/concurrent.h>
#include <nx/utils/scope_guard.h>

#include "ec2_thread_pool.h"
#include <database/db_manager.h>
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"
#include <transaction/binary_transaction_serializer.h>
#include <api/app_server_connection.h>
#include <api/model/password_data.h>
#include <ec_connection_notification_manager.h>
#include <core/resource/param.h>
#include "ec_connection_audit_manager.h"

#include <nx/vms/event/rule.h>
#include "nx_ec/data/api_conversion_functions.h"

#include "utils/common/threadqueue.h"
#include <utils/crypt/symmetrical.h>
#include <transaction/message_bus_adapter.h>

namespace ec2 {

typedef QnUnsafeQueue<std::function<void()>> PostProcessList;

namespace detail { class ServerQueryProcessor; }

struct ServerQueryProcessorAccess
{
    // TODO: for compatibility with ClientQueryProcessor. It is not need actually. Remove it.
    QString userName() const { return QString(); }

    ServerQueryProcessorAccess(detail::QnDbManager* db, TransactionMessageBusAdapter* messageBus):
        m_db(db),
        m_messageBus(messageBus)
    {
    }

    virtual ~ServerQueryProcessorAccess()
    {
        waitForAsyncTasks();
    }

    void waitForAsyncTasks()
    {
        QnMutexLocker lock(&m_runnigAsyncOperationsMutex);
        while (m_runnigAsyncOperationsCount > 0)
            m_runnigAsyncOperationsCondition.wait(lock.mutex());
    }

    void incRunningAsyncOperationsCount()
    {
        QnMutexLocker lock(&m_runnigAsyncOperationsMutex);
        ++m_runnigAsyncOperationsCount;
    }

    void decRunningAsyncOperationsCount()
    {
        QnMutexLocker lock(&m_runnigAsyncOperationsMutex);
        --m_runnigAsyncOperationsCount;
        m_runnigAsyncOperationsCondition.wakeOne();
    }

    detail::ServerQueryProcessor getAccess(const Qn::UserAccessData userAccessData);

    detail::QnDbManager* getDb() const { return m_db; }
    TransactionMessageBusAdapter* messageBus() { return m_messageBus; }
    PostProcessList* postProcessList() { return &m_postProcessList; }
    QnMutex* updateMutex() { return &m_updateMutex; }
    QnCommonModule* commonModule() const { return m_messageBus->commonModule();  }
private:
    detail::QnDbManager* m_db;
    TransactionMessageBusAdapter* m_messageBus;
    PostProcessList m_postProcessList;
    QnMutex m_updateMutex;
    QnMutex m_runnigAsyncOperationsMutex;
    QnWaitCondition m_runnigAsyncOperationsCondition;
    int m_runnigAsyncOperationsCount = 0;
};

namespace detail {
namespace aux {

struct AuditData
{
    ECConnectionAuditManager* auditManager;
    ECConnectionNotificationManager* notificationManager;
    QnAuthSession authSession;
    Qn::UserAccessData userAccessData;

    AuditData(
        ECConnectionAuditManager* auditManager,
        ECConnectionNotificationManager* notificationManager,
        const QnAuthSession& authSession,
        const Qn::UserAccessData& userAccessData)
        :
        auditManager(auditManager),
        notificationManager(notificationManager),
        authSession(authSession),
        userAccessData(userAccessData)
    {}
};

template<class DataType>
void triggerNotification(
    const AuditData& auditData,
    const QnTransaction<DataType>& tran)
{
    // Add audit record before notification to ensure removed resource is still alive.
    if (auditData.auditManager &&
        auditData.userAccessData != Qn::kSystemAccess) //< don't add to audit log if it's server side update
    {
        auditData.auditManager->addAuditRecord(
            tran.command,
            tran.params,
            auditData.authSession);
    }

    // Notification manager is null means startReceivingNotification isn't called yet
    if (auditData.notificationManager)
        auditData.notificationManager->triggerNotification(tran, NotificationSource::Local);

} // namespace aux
} // namespace detail

struct PostProcessTransactionFunction
{
    template<class T>
    void operator()(
        TransactionMessageBusAdapter* messageBus,
        const aux::AuditData& auditData,
        const QnTransaction<T>& tran) const;
};

template<typename RequestData>
inline void fixRequestDataIfNeeded(RequestData* const /*requestData*/)
{
}

inline void fixRequestDataIfNeeded(nx::vms::api::UserData* const userData)
{
    if (userData->isCloud)
    {
        if (userData->name.isEmpty())
            userData->name = userData->email;

        if (userData->digest.isEmpty())
            userData->digest = nx::vms::api::UserData::kCloudPasswordStub;
    }
}

inline void fixRequestDataIfNeeded(nx::vms::api::UserDataEx* const userDataEx)
{
    if (!userDataEx->password.isEmpty())
    {
        const auto hashes = PasswordData::calculateHashes(userDataEx->name, userDataEx->password,
            userDataEx->isLdap);

        userDataEx->realm = hashes.realm;
        userDataEx->hash = hashes.passwordHash;
        userDataEx->digest = hashes.passwordDigest;
        userDataEx->cryptSha512Hash = hashes.cryptSha512Hash;
    }

    fixRequestDataIfNeeded(static_cast<nx::vms::api::UserData* const>(userDataEx));
}

inline void fixRequestDataIfNeeded(nx::vms::api::ResourceParamData* const paramData)
{
    if (paramData->name == Qn::CAMERA_CREDENTIALS_PARAM_NAME
    || paramData->name == Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME)
    {
        paramData->value = nx::utils::encodeHexStringFromStringAES128CBC(paramData->value);
    }
}

template <typename T>
auto amendTranIfNeeded(const QnTransaction<T>& tran)
{
    return tran;
}

inline QnTransaction<nx::vms::api::UserData> amendTranIfNeeded(
    const QnTransaction<nx::vms::api::UserDataEx>& originalTran)
{
    QnTransaction<nx::vms::api::UserData> resultTran(static_cast<const QnAbstractTransaction&>(
        originalTran));
    auto originalParams = originalTran.params;
    fixRequestDataIfNeeded(&originalParams);
    resultTran.params = nx::vms::api::UserData(originalParams);

    return resultTran;
}

inline QnTransaction<nx::vms::api::ResourceParamData> amendTranIfNeeded(
    const QnTransaction<nx::vms::api::ResourceParamData>& originalTran)
{
    auto resultTran = originalTran;
    fixRequestDataIfNeeded(&resultTran.params);

    return resultTran;
}

inline QnTransaction<nx::vms::api::ResourceParamWithRefData> amendTranIfNeeded(
    const QnTransaction<nx::vms::api::ResourceParamWithRefData>& originalTran)
{
    auto resultTran = originalTran;
    fixRequestDataIfNeeded(static_cast<nx::vms::api::ResourceParamData* const>(&resultTran.params));

    return resultTran;
}

template<typename T>
void amendOutputDataIfNeeded(const Qn::UserAccessData&, T*)
{
}

inline void amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    nx::vms::api::ResourceParamData* paramData)
{
    if (paramData->name == Qn::CAMERA_CREDENTIALS_PARAM_NAME
    || paramData->name == Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME)
    {
        auto decryptedValue = nx::utils::decodeStringFromHexStringAES128CBC(paramData->value);
        if (accessData == Qn::kSystemAccess)
            paramData->value = decryptedValue;
        else
            paramData->value = decryptedValue.replace(QRegExp(":.*$"), ":******");
    }
}

inline void amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    nx::vms::api::ResourceParamWithRefData* paramData)
{
    return amendOutputDataIfNeeded(
        accessData,
        static_cast<nx::vms::api::ResourceParamData*>(paramData));
}

inline void amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    std::vector<nx::vms::api::ResourceParamData>* paramDataList)
{
    for (auto& paramData: *paramDataList)
        amendOutputDataIfNeeded(accessData, &paramData);
}

inline void amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    std::vector<nx::vms::api::ResourceParamWithRefData>* paramWithRefDataList)
{
    for (auto& paramData: *paramWithRefDataList)
    {
        amendOutputDataIfNeeded(
            accessData,
            static_cast<nx::vms::api::ResourceParamData*>(&paramData));
    }
}

class ServerQueryProcessor
{
public:
     virtual ~ServerQueryProcessor() {}

    ServerQueryProcessor(
        ServerQueryProcessorAccess* owner,
        const Qn::UserAccessData &userAccessData)
        :
        m_owner(owner),
        m_db(owner->getDb(), userAccessData),
        m_auditManager(nullptr)
    {
    }

    template<class InputData, class HandlerType>
    void processUpdateAsync(
        ApiCommand::Value cmdCode, InputData input, HandlerType handler)
    {
        QnTransaction<InputData> tran = createTransaction(cmdCode, std::move(input));
        processUpdateAsync(tran, std::move(handler));
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class QueryDataType, class HandlerType>
    void processUpdateAsync(
        QnTransaction<QueryDataType>& tran, HandlerType handler, void* /*dummy*/ = 0)
    {
        m_owner->incRunningAsyncOperationsCount();
        nx::utils::concurrent::run(Ec2ThreadPool::instance(),
            [self = *this, tran = std::move(tran), handler = std::move(handler)]() mutable
            {
                self.executeTranCall(
                    tran,
                    handler,
                    [self](
                        QnTransaction<QueryDataType>& tran,
                        PostProcessList* const transactionsPostProcessList) mutable -> ErrorCode
                    {
                        auto errorCode = self.processUpdateSync(tran, transactionsPostProcessList);
                        return errorCode;
                    });
                self.m_owner->decRunningAsyncOperationsCount();
            });
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<nx::vms::api::IdDataList>& tran, HandlerType handler)
    {
        switch (tran.command)
        {
            case ApiCommand::removeStorages:
                return processMultiUpdateAsync<nx::vms::api::IdDataList, nx::vms::api::IdData>(
                    tran, handler, ApiCommand::removeStorage);
            case ApiCommand::removeResources:
                return processMultiUpdateAsync<nx::vms::api::IdDataList, nx::vms::api::IdData>(
                    tran, handler, ApiCommand::removeResource);
            default:
                NX_ASSERT(false, "Not implemented", Q_FUNC_INFO);
        }
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<nx::vms::api::IdData>& tran, HandlerType handler)
    {
        return processUpdateAsync(tran, handler, 0); //< call default handler
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<nx::vms::api::LicenseDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::addLicenses);
        return processMultiUpdateAsync<nx::vms::api::LicenseDataList, nx::vms::api::LicenseData>(
            tran, handler, ApiCommand::addLicense);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<nx::vms::api::LayoutDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveLayouts);
        return processMultiUpdateAsync<nx::vms::api::LayoutDataList, nx::vms::api::LayoutData>(
            tran,
            handler,
            ApiCommand::saveLayout);
    }

    /**
    * Execute transaction.
    * Transaction executed locally and broadcast through the whole cluster.
    * @param handler Called upon request completion. Functor(ErrorCode).
    */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<nx::vms::api::UserDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveUsers);
        return processMultiUpdateAsync<nx::vms::api::UserDataList, nx::vms::api::UserData>(
            tran, handler, ApiCommand::saveUser);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<nx::vms::api::CameraDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveCameras);
        return processMultiUpdateAsync<nx::vms::api::CameraDataList, nx::vms::api::CameraData>(
            tran, handler, ApiCommand::saveCamera);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<nx::vms::api::StorageDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveStorages);
        return processMultiUpdateAsync<nx::vms::api::StorageDataList, nx::vms::api::StorageData>(
            tran, handler, ApiCommand::saveStorage);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(
        QnTransaction<nx::vms::api::CameraAttributesDataList>& tran,
        HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveCameraUserAttributesList);
        return processMultiUpdateAsync<
            nx::vms::api::CameraAttributesDataList,
            nx::vms::api::CameraAttributesData>(
                tran,
                handler,
                ApiCommand::saveCameraUserAttributes);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(
        QnTransaction<nx::vms::api::MediaServerUserAttributesDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveMediaServerUserAttributesList);
        return processMultiUpdateAsync<
            nx::vms::api::MediaServerUserAttributesDataList, nx::vms::api::MediaServerUserAttributesData>(
                tran, handler, ApiCommand::saveMediaServerUserAttributes);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(
        QnTransaction<nx::vms::api::ResourceParamWithRefDataList>& tran,
        HandlerType handler)
    {
        switch (tran.command)
        {
            case ApiCommand::setResourceParams:
                return processMultiUpdateAsync<
                    nx::vms::api::ResourceParamWithRefDataList,
                    nx::vms::api::ResourceParamWithRefData>(
                    tran,
                    handler,
                    ApiCommand::setResourceParam);
            case ApiCommand::removeResourceParams:
                return processMultiUpdateAsync<
                    nx::vms::api::ResourceParamWithRefDataList,
                    nx::vms::api::ResourceParamWithRefData>(
                    tran,
                    handler,
                    ApiCommand::removeResourceParam);
            default:
                NX_ASSERT(0, "Not implemented!", Q_FUNC_INFO);
        }
    }

    /**
     * Asynchronously fetch data from DB.
     * @param handler Functor(ErrorCode, OutputData).
     * TODO #ak Let compiler guess template params.
     */
    template<class InputData, class OutputData, class HandlerType>
    void processQueryAsync(ApiCommand::Value /*cmdCode*/, InputData input, HandlerType handler)
    {
        QnDbManagerAccess accessDataCopy(m_db);
        m_owner->incRunningAsyncOperationsCount();
        nx::utils::concurrent::run(Ec2ThreadPool::instance(),
            [self = *this, accessDataCopy, input, handler]() mutable
            {
                OutputData output;
                const ErrorCode errorCode = accessDataCopy.doQuery(input, output);
                amendOutputDataIfNeeded(self.m_db.userAccessData(), &output);
                handler(errorCode, output);
                self.m_owner->decRunningAsyncOperationsCount();
            });
    }

    void setAuditData(ECConnectionAuditManager* auditManager, const QnAuthSession& authSession);

private:
    aux::AuditData createAuditDataCopy()
    {
        const auto& ec2Connection = m_owner->commonModule()->ec2Connection();
        return aux::AuditData(
            m_auditManager,
            ec2Connection ? ec2Connection->notificationManager() : nullptr,
            m_authSession,
            m_db.userAccessData());
    }

    /**
     * @param syncFunction ErrorCode(QnTransaction<QueryDataType>&,
     *     PostProcessList*)
     */
    template<class QueryDataType, class CompletionHandlerType, class SyncFunctionType>
    void executeTranCall(
        QnTransaction<QueryDataType>& tran,
        CompletionHandlerType completionHandler,
        SyncFunctionType syncFunction)
    {
        ErrorCode errorCode = ErrorCode::ok;
        auto scopeGuard = nx::utils::makeScopeGuard([&](){ completionHandler(errorCode); });

        PostProcessList* transactionsPostProcessList = m_owner->postProcessList();
        // Starting transaction.
        QnMutexLocker lock(m_owner->updateMutex());
        {
            std::unique_ptr<detail::QnDbManager::QnDbTransactionLocker> dbTran;
            PostProcessList localPostProcessList;

            if (ApiCommand::isPersistent(tran.command))
            {
                dbTran.reset(new detail::QnDbManager::QnDbTransactionLocker(
                    m_db.getTransaction()));
                errorCode = syncFunction(tran, &localPostProcessList);
                if (errorCode != ErrorCode::ok)
                    return;
                if (!dbTran->commit())
                {
                    errorCode = ErrorCode::dbError;
                    return;
                }
            }
            else
            {
                if (!getTransactionDescriptorByTransaction(tran)->checkSavePermissionFunc(m_owner->commonModule(), m_db.userAccessData(), tran.params))
                {
                    errorCode = ErrorCode::forbidden;
                    return;
                }
                localPostProcessList.push(std::bind(
                    PostProcessTransactionFunction(), m_owner->messageBus(), createAuditDataCopy(), tran));
            }

            std::function<void()> postProcessAction;
            while (localPostProcessList.pop(postProcessAction))
                transactionsPostProcessList->push(postProcessAction);
        }

        // Sending transactions.
        std::function<void()> postProcessAction;
        while (transactionsPostProcessList->pop(postProcessAction))
            postProcessAction();

        // Handler is invoked asynchronously.
    }

    template<class HandlerType>
    void removeResourceAsync(
        QnTransaction<nx::vms::api::IdData>& tran,
        ApiObjectType resourceType,
        HandlerType handler)
    {
        using namespace std::placeholders;
        executeTranCall(
            tran,
            handler,
            std::bind(&ServerQueryProcessor::removeResourceSync, this, _1, resourceType, _2));
    }

    ErrorCode removeHelper(
        const QnUuid& id,
        ApiCommand::Value command,
        PostProcessList* const transactionsPostProcessList,
        TransactionType::Value transactionType = TransactionType::Regular);

    ErrorCode removeObjAttrHelper(
        const QnUuid& id,
        ApiCommand::Value command,
        PostProcessList* const transactionsPostProcessList);

    ErrorCode removeObjParamsHelper(
        const QnTransaction<nx::vms::api::IdData>& tran,
        const AbstractECConnectionPtr& connection,
        PostProcessList* const transactionsPostProcessList);

    ErrorCode removeUserAccessRightsHelper(
        const QnUuid& id,
        PostProcessList* const transactionsPostProcessList);

    ErrorCode removeResourceStatusHelper(
        const QnUuid& id,
        PostProcessList* const transactionsPostProcessList,
        TransactionType::Value transactionType = TransactionType::Regular);

    class AccessRightGrant
    {
    public:
        AccessRightGrant(
            ServerQueryProcessor* owner,
            Qn::UserAccessData accessRight)
            :
            m_owner(owner),
            m_previousAccessRight(owner->m_db.userAccessData())
        {
            m_owner->m_db.setUserAccessData(accessRight);
        }

        ~AccessRightGrant()
        {
            m_owner->m_db.setUserAccessData(m_previousAccessRight);
        }
    private:
        ServerQueryProcessor* m_owner;
        Qn::UserAccessData m_previousAccessRight;
    };

    ErrorCode removeResourceSync(
        QnTransaction<nx::vms::api::IdData>& tran,
        ApiObjectType resourceType,
        PostProcessList* const transactionsPostProcessList)
    {
        ErrorCode errorCode = removeResourceNestedObjectsSync(tran, resourceType, transactionsPostProcessList);
        if (errorCode != ErrorCode::ok)
            return errorCode;
        return processUpdateSync(tran, transactionsPostProcessList, 0);
    }

    ErrorCode removeResourceNestedObjectsSync(
        QnTransaction<nx::vms::api::IdData>& tran,
        ApiObjectType resourceType,
        PostProcessList* const transactionsPostProcessList)
    {
        // Remove nested objects without checking actual access rights. Only main remove transaction will be validated.
        AccessRightGrant grant(this, Qn::kSystemAccess);

        ErrorCode errorCode = ErrorCode::ok;
        auto connection = m_owner->commonModule()->ec2Connection();

        #define RUN_AND_CHECK_ERROR(EXPR, MESSAGE) do \
        { \
            ErrorCode errorCode = (EXPR); \
            if (errorCode != ErrorCode::ok) \
            { \
                NX_WARNING(this, (MESSAGE)); \
                return errorCode; \
            } \
        } while (0)

        switch (resourceType)
        {
            case ApiObject_Camera:
            {
                RUN_AND_CHECK_ERROR(
                    removeObjAttrHelper(
                        tran.params.id,
                        ApiCommand::removeCameraUserAttributes,
                        transactionsPostProcessList),
                    lit("Remove camera attributes failed"));

                RUN_AND_CHECK_ERROR(
                    removeObjParamsHelper(tran, connection, transactionsPostProcessList),
                    lit("Remove camera params failed"));

                RUN_AND_CHECK_ERROR(
                    removeResourceStatusHelper(
                        tran.params.id,
                        transactionsPostProcessList),
                    lit("Remove resource access status failed"));

                break;
            }

            case ApiObject_Server:
            {
                RUN_AND_CHECK_ERROR(
                    removeObjAttrHelper(
                        tran.params.id,
                        ApiCommand::removeServerUserAttributes,
                        transactionsPostProcessList),
                    lit("Remove server attrs failed"));

                RUN_AND_CHECK_ERROR(
                    removeObjParamsHelper(tran, connection, transactionsPostProcessList),
                    lit("Remove server params failed"));

                RUN_AND_CHECK_ERROR(
                    processMultiUpdateSync(
                        ApiCommand::removeResource,
                        tran.transactionType,
                        m_db
                            .getNestedObjectsNoLock(ApiObjectInfo(resourceType, tran.params.id))
                            .toIdList(),
                        transactionsPostProcessList),
                    lit("Remove server child resources failed"));

                RUN_AND_CHECK_ERROR(
                    removeResourceStatusHelper(
                        tran.params.id,
                        transactionsPostProcessList,
                        TransactionType::Local),
                    lit("Remove resource status failed"));

                break;
            }

            case ApiObject_Storage:
            {
                RUN_AND_CHECK_ERROR(
                    removeObjParamsHelper(tran, connection, transactionsPostProcessList),
                    lit("Remove storage params failed"));

                RUN_AND_CHECK_ERROR(
                    removeResourceStatusHelper(
                        tran.params.id,
                        transactionsPostProcessList,
                        tran.transactionType),
                    lit("Remove resource status failed"));

                break;
            }

            case ApiObject_User:
            {
                RUN_AND_CHECK_ERROR(
                    removeObjParamsHelper(tran, connection, transactionsPostProcessList),
                    lit("Remove user params failed"));

                RUN_AND_CHECK_ERROR(
                    processMultiUpdateSync(
                        ApiCommand::removeLayout,
                        tran.transactionType,
                        m_db
                            .getNestedObjectsNoLock(ApiObjectInfo(resourceType, tran.params.id))
                            .toIdList(),
                        transactionsPostProcessList),
                    lit("Remove user child resources failed"));

                // Removing owned layout tours
                const auto tourIds = m_db.getLayoutToursNoLock(tran.params.id);
                RUN_AND_CHECK_ERROR(
                    processMultiUpdateSync(
                        ApiCommand::removeLayoutTour,
                        tran.transactionType,
                        tourIds,
                        transactionsPostProcessList),
                    lit("Remove user child layout tours failed"));

                // Removing autogenerated layouts, belonging to layout tours
                for (const auto& tourId: tourIds)
                {
                    RUN_AND_CHECK_ERROR(
                        processMultiUpdateSync(
                            ApiCommand::removeLayout,
                            tran.transactionType,
                            m_db
                            .getNestedObjectsNoLock(ApiObjectInfo(ApiObject_User, tourId.id))
                            .toIdList(),
                            transactionsPostProcessList),
                        lit("Remove autogenerated tour's layouts failed"));
                }

                RUN_AND_CHECK_ERROR(
                    removeUserAccessRightsHelper(
                        tran.params.id,
                        transactionsPostProcessList),
                    lit("Remove user access rights failed"));

                break;
            }

            case ApiObjectUserRole:
            {
                RUN_AND_CHECK_ERROR(
                    removeUserAccessRightsHelper(
                        tran.params.id,
                        transactionsPostProcessList),
                    lit("Remove user access rights failed"));

                break;
            }

            case ApiObject_Videowall:
            {
                RUN_AND_CHECK_ERROR(
                    processMultiUpdateSync(
                        ApiCommand::removeLayout,
                        tran.transactionType,
                        m_db
                            .getNestedObjectsNoLock(ApiObjectInfo(resourceType, tran.params.id))
                            .toIdList(),
                        transactionsPostProcessList),
                    lit("Remove videowall child resources failed"));

                break;
            }

            default:
                NX_ASSERT(0);
        }

        return errorCode;

        #undef RUN_AND_CHECK_ERROR

    }

public:
    ErrorCode processUpdateSync(
        QnTransaction<nx::vms::api::ResetEventRulesData>& tran,
        PostProcessList* const transactionsPostProcessList,
        int /*dummy*/ = 0)
    {
        ErrorCode errorCode = processMultiUpdateSync(
            ApiCommand::removeEventRule,
            tran.transactionType,
            m_db.getObjectsNoLock(ApiObject_BusinessRule).toIdList(),
            transactionsPostProcessList);

        if (errorCode != ErrorCode::ok)
            return errorCode;

        nx::vms::api::EventRuleDataList defaultRules;
        fromResourceListToApi(nx::vms::event::Rule::getDefaultRules(), defaultRules);

        return processMultiUpdateSync(
            ApiCommand::saveEventRule,
            tran.transactionType,
            defaultRules,
            transactionsPostProcessList);
    }

    template<class QueryDataType>
    ErrorCode processUpdateSync(
        QnTransaction<QueryDataType>& tran,
        PostProcessList* const transactionsPostProcessList,
        int /*dummy*/ = 0)
    {
        NX_ASSERT(ApiCommand::isPersistent(tran.command));

        detail::PersistentStorage persistentDb(m_db.db());
        auto amendedTran = amendTranIfNeeded(tran);
        amendedTran.transactionType = getTransactionDescriptorByTransaction(amendedTran)
            ->getTransactionTypeFunc(
                m_db.db()->commonModule(),
                amendedTran.params,
                &persistentDb);

        if (amendedTran.transactionType == TransactionType::Unknown)
            return ErrorCode::forbidden;

        m_db.db()->transactionLog()->fillPersistentInfo(amendedTran);
        QByteArray serializedTran = m_owner->messageBus()->ubjsonTranSerializer()
            ->serializedTransaction(amendedTran);

        ErrorCode errorCode = m_db.executeTransactionNoLock(amendedTran, serializedTran);
        NX_ASSERT(errorCode != ErrorCode::containsBecauseSequence
            && errorCode != ErrorCode::containsBecauseTimestamp);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        transactionsPostProcessList->push(std::bind(
            PostProcessTransactionFunction(),
            m_owner->messageBus(),
            createAuditDataCopy(),
            amendedTran));

        return errorCode;
    }

    ErrorCode processUpdateSync(
        QnTransaction<nx::vms::api::IdData>& tran,
        PostProcessList* const transactionsPostProcessList)
    {
        switch (tran.command)
        {
            case ApiCommand::removeMediaServer:
                return removeResourceSync(tran, ApiObject_Server, transactionsPostProcessList);
            case ApiCommand::removeUser:
                return removeResourceSync(tran, ApiObject_User, transactionsPostProcessList);
            case ApiCommand::removeCamera:
                return removeResourceSync(tran, ApiObject_Camera, transactionsPostProcessList);
            case ApiCommand::removeStorage:
                return removeResourceSync(tran, ApiObject_Storage, transactionsPostProcessList);
            case ApiCommand::removeVideowall:
                return removeResourceSync(tran, ApiObject_Videowall, transactionsPostProcessList);
            case ApiCommand::removeUserRole:
                return removeResourceSync(tran, ApiObjectUserRole, transactionsPostProcessList);
            case ApiCommand::removeAnalyticsPlugin:
            {
                return removeResourceSync(
                    tran,
                    ApiObject_AnalyticsPlugin,
                    transactionsPostProcessList);
            }
            case ApiCommand::removeAnalyticsEngine:
            {
                return removeResourceSync(
                    tran,
                    ApiObject_AnalyticsEngine,
                    transactionsPostProcessList);
            }
            case ApiCommand::removeResource:
            {
                QnTransaction<nx::vms::api::IdData> updatedTran = tran;
                switch(m_db.getObjectTypeNoLock(tran.params.id))
                {
                    case ApiObject_Server:
                        updatedTran.command = ApiCommand::removeMediaServer;
                        break;
                    case ApiObject_Camera:
                        updatedTran.command = ApiCommand::removeCamera;
                        break;
                    case ApiObject_Storage:
                        updatedTran.command = ApiCommand::removeStorage;
                        break;
                    case ApiObject_User:
                        updatedTran.command = ApiCommand::removeUser;
                        break;
                    case ApiObject_Layout:
                        updatedTran.command = ApiCommand::removeLayout;
                        break;
                    case ApiObject_Videowall:
                        updatedTran.command = ApiCommand::removeVideowall;
                        break;
                    case ApiObject_WebPage:
                        updatedTran.command = ApiCommand::removeWebPage;
                        break;
                    case ApiObject_BusinessRule:
                        updatedTran.command = ApiCommand::removeEventRule;
                        break;
                    case ApiObject_AnalyticsPlugin:
                        updatedTran.command = ApiCommand::removeAnalyticsPlugin;
                        break;
                    case ApiObject_AnalyticsEngine:
                        updatedTran.command = ApiCommand::removeAnalyticsEngine;
                        break;
                    default:
                        return processUpdateSync(tran, transactionsPostProcessList, 0);
                }
                return processUpdateSync(updatedTran, transactionsPostProcessList); //< calling recursively
            }
            default:
                return processUpdateSync(tran, transactionsPostProcessList, 0);
        }
    }

    template<class SubDataType>
    ErrorCode processMultiUpdateSync(
        ApiCommand::Value command,
        TransactionType::Value transactionType,
        const std::vector<SubDataType>& nestedList,
        PostProcessList* const transactionsPostProcessList)
    {
        for(const SubDataType& data: nestedList)
        {
            QnTransaction<SubDataType> subTran = createTransaction(command, data);
            subTran.transactionType = transactionType;
            ErrorCode errorCode = processUpdateSync(subTran, transactionsPostProcessList);
            if (errorCode != ErrorCode::ok)
                return errorCode;
        }

        return ErrorCode::ok;
    }

    template<class QueryDataType, class SubDataType, class HandlerType>
    void processMultiUpdateAsync(
        QnTransaction<QueryDataType>& multiTran,
        HandlerType handler,
        ApiCommand::Value subCommand)
    {
        m_owner->incRunningAsyncOperationsCount();
        nx::utils::concurrent::run(Ec2ThreadPool::instance(),
            [self = *this, multiTran = std::move(multiTran), handler = std::move(handler), subCommand]() mutable
            {
                self.executeTranCall(
                    multiTran,
                    handler,
                    [self, subCommand](
                        QnTransaction<QueryDataType>& multiTran,
                        PostProcessList* const transactionsPostProcessList) mutable -> ErrorCode
                    {
                        auto errorCode = self.processMultiUpdateSync(
                            subCommand, multiTran.transactionType, multiTran.params,
                            transactionsPostProcessList);
                        return errorCode;
                    });
                self.m_owner->decRunningAsyncOperationsCount();
            });
    }

private:
    ServerQueryProcessorAccess* m_owner;
    QnDbManagerAccess m_db;
    ECConnectionAuditManager* m_auditManager;
    QnAuthSession m_authSession;

    template<typename DataType>
    QnTransaction<DataType> createTransaction(
        ApiCommand::Value command,
        DataType data)
    {
        QnTransaction<DataType> transaction(command, m_owner->commonModule()->moduleGUID(), std::move(data));
        transaction.historyAttributes.author = m_db.userAccessData().userId;
        return transaction;
    }
};

template<class T>
void PostProcessTransactionFunction::operator()(
    TransactionMessageBusAdapter* messageBus,
    const aux::AuditData& auditData,
    const QnTransaction<T>& tran) const
{
    messageBus->sendTransaction(tran);
    auto amendedTran = tran;
    amendOutputDataIfNeeded(Qn::kSystemAccess, &amendedTran.params);
    aux::triggerNotification(auditData, amendedTran);
}

} // namespace detail

} // namespace ec2
