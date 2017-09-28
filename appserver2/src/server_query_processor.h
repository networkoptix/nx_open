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
#include <ec_connection_notification_manager.h>
#include "ec_connection_audit_manager.h"

#include <nx/vms/event/rule.h>
#include "nx_ec/data/api_conversion_functions.h"

#include "utils/common/threadqueue.h"
#include <transaction/message_bus_adapter.h>

namespace ec2 {

typedef QnSafeQueue<std::function<void()>> PostProcessList;

namespace detail { class ServerQueryProcessor; }

struct ServerQueryProcessorAccess
{
    ServerQueryProcessorAccess(detail::QnDbManager* db, TransactionMessageBusAdapter* messageBus):
        m_db(db),
        m_messageBus(messageBus)
    {
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
                        return self.processUpdateSync(tran, transactionsPostProcessList);
                    });
            });
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<ApiIdDataList>& tran, HandlerType handler)
    {
        switch (tran.command)
        {
            case ApiCommand::removeStorages:
                return processMultiUpdateAsync<ApiIdDataList, ApiIdData>(
                    tran, handler, ApiCommand::removeStorage);
            case ApiCommand::removeResources:
                return processMultiUpdateAsync<ApiIdDataList, ApiIdData>(
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
    void processUpdateAsync(QnTransaction<ApiIdData>& tran, HandlerType handler)
    {
        return processUpdateAsync(tran, handler, 0); //< call default handler
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<ApiLicenseDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::addLicenses);
        return processMultiUpdateAsync<ApiLicenseDataList, ApiLicenseData>(
            tran, handler, ApiCommand::addLicense);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<ApiLayoutDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveLayouts);
        return processMultiUpdateAsync<ApiLayoutDataList, ApiLayoutData>(
            tran, handler, ApiCommand::saveLayout);
    }

    /**
    * Execute transaction.
    * Transaction executed locally and broadcast through the whole cluster.
    * @param handler Called upon request completion. Functor(ErrorCode).
    */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<ApiUserDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveUsers);
        return processMultiUpdateAsync<ApiUserDataList, ApiUserData>(
            tran, handler, ApiCommand::saveUser);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<ApiCameraDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveCameras);
        return processMultiUpdateAsync<ApiCameraDataList, ApiCameraData>(
            tran, handler, ApiCommand::saveCamera);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<ApiStorageDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveStorages);
        return processMultiUpdateAsync<ApiStorageDataList, ApiStorageData>(
            tran, handler, ApiCommand::saveStorage);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(QnTransaction<ApiCameraAttributesDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveCameraUserAttributesList);
        return processMultiUpdateAsync<ApiCameraAttributesDataList, ApiCameraAttributesData>(
            tran, handler, ApiCommand::saveCameraUserAttributes);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(
        QnTransaction<ApiMediaServerUserAttributesDataList>& tran, HandlerType handler)
    {
        NX_ASSERT(tran.command == ApiCommand::saveMediaServerUserAttributesList);
        return processMultiUpdateAsync<
            ApiMediaServerUserAttributesDataList, ApiMediaServerUserAttributesData>(
                tran, handler, ApiCommand::saveMediaServerUserAttributes);
    }

    /**
     * Execute transaction.
     * Transaction executed locally and broadcast through the whole cluster.
     * @param handler Called upon request completion. Functor(ErrorCode).
     */
    template<class HandlerType>
    void processUpdateAsync(
        QnTransaction<ApiResourceParamWithRefDataList>& tran, HandlerType handler)
    {
        switch (tran.command)
        {
            case ApiCommand::setResourceParams:
                return processMultiUpdateAsync<
                    ApiResourceParamWithRefDataList, ApiResourceParamWithRefData>(
                        tran, handler, ApiCommand::setResourceParam);
            case ApiCommand::removeResourceParams:
                return processMultiUpdateAsync<
                    ApiResourceParamWithRefDataList, ApiResourceParamWithRefData>(
                        tran, handler, ApiCommand::removeResourceParam);
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
    void processQueryAsync(ApiCommand::Value cmdCode, InputData input, HandlerType handler)
    {
        QN_UNUSED(cmdCode);

        QnDbManagerAccess accessDataCopy(m_db);
        nx::utils::concurrent::run(Ec2ThreadPool::instance(),
            [accessDataCopy, input, handler]() mutable
            {
                OutputData output;
                const ErrorCode errorCode = accessDataCopy.doQuery(input, output);
                handler(errorCode, output);
            });
    }

    /**
     * Asynchronously fetch data from DB.
     * @param handler Functor(ErrorCode, OutputData).
     * TODO #ak Let compiler guess template params.
     */
    template<class OutputData, class InputParamType1, class InputParamType2, class HandlerType>
    void processQueryAsync(
        ApiCommand::Value cmdCode, InputParamType1 input1, InputParamType2 input2,
        HandlerType handler)
    {
        QN_UNUSED(cmdCode);

        QnDbManagerAccess accessDataCopy(m_db);
        nx::utils::concurrent::run(Ec2ThreadPool::instance(),
            [accessDataCopy, input1, input2, handler]()
            {
                OutputData output;
                const ErrorCode errorCode = accessDataCopy.doQuery(
                    input1, input2, output);
                handler(errorCode, output);
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
        auto scopeGuard = makeScopeGuard([&](){ completionHandler(errorCode); });

        PostProcessList* transactionsPostProcessList = m_owner->postProcessList();
        // Starting transaction.
        {
            QnMutexLocker lock(m_owner->updateMutex());
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
            while (localPostProcessList.pop(postProcessAction, 0))
                transactionsPostProcessList->push(postProcessAction);
        }

        // Sending transactions.
        std::function<void()> postProcessAction;
        while (transactionsPostProcessList->pop(postProcessAction, 0))
            postProcessAction();

        // Handler is invoked asynchronously.
    }

    template<class HandlerType>
    void removeResourceAsync(
        QnTransaction<ApiIdData>& tran,
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
        const QnTransaction<ApiIdData>& tran,
        const AbstractECConnectionPtr& connection,
        PostProcessList* const transactionsPostProcessList);

    ErrorCode removeObjAccessRightsHelper(
        const QnUuid& id,
        PostProcessList* const transactionsPostProcessList);

    ErrorCode removeResourceStatusHelper(
        const QnUuid& id,
        PostProcessList* const transactionsPostProcessList,
        TransactionType::Value transactionType = TransactionType::Regular);

    ErrorCode removeResourceSync(
        QnTransaction<ApiIdData>& tran,
        ApiObjectType resourceType,
        PostProcessList* const transactionsPostProcessList)
    {
        ErrorCode errorCode = ErrorCode::ok;
        auto connection = m_owner->commonModule()->ec2Connection();

        #define RUN_AND_CHECK_ERROR(EXPR, MESSAGE) do \
        { \
            ErrorCode errorCode = (EXPR); \
            if (errorCode != ErrorCode::ok) \
            { \
                NX_LOG((MESSAGE), cl_logWARNING); \
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

        RUN_AND_CHECK_ERROR(
            removeObjAccessRightsHelper(
                tran.params.id,
                transactionsPostProcessList),
            lit("Remove resource access rights failed"));

        if(errorCode != ErrorCode::ok)
            return errorCode;

        #undef RUN_AND_CHECK_ERROR

        return processUpdateSync(tran, transactionsPostProcessList, 0);
    }

public:
    ErrorCode processUpdateSync(
        QnTransaction<ApiResetBusinessRuleData>& tran,
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

        ApiBusinessRuleDataList defaultRules;
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

        tran.transactionType = getTransactionDescriptorByTransaction(tran)->getTransactionTypeFunc(m_db.db()->commonModule(), tran.params, m_db.db());
        if (tran.transactionType == TransactionType::Unknown)
            return ErrorCode::forbidden;

        m_db.db()->transactionLog()->fillPersistentInfo(tran);
        QByteArray serializedTran = m_owner->messageBus()->ubjsonTranSerializer()->serializedTransaction(tran);

        ErrorCode errorCode =
            m_db.executeTransactionNoLock(tran, serializedTran);
        NX_ASSERT(errorCode != ErrorCode::containsBecauseSequence
            && errorCode != ErrorCode::containsBecauseTimestamp);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        transactionsPostProcessList->push(std::bind(PostProcessTransactionFunction(), m_owner->messageBus(), createAuditDataCopy(), tran));

        return errorCode;
    }

    ErrorCode processUpdateSync(
        QnTransaction<ApiIdData>& tran,
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
            case ApiCommand::removeResource:
            {
                QnTransaction<ApiIdData> updatedTran = tran;
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
                        return self.processMultiUpdateSync(
                            subCommand, multiTran.transactionType, multiTran.params,
                            transactionsPostProcessList);
                    });
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
    aux::triggerNotification(auditData, tran);
}

} // namespace detail

} // namespace ec2
