#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QDebug>

#include <utils/common/scoped_thread_rollback.h>
#include <nx/fusion/model_functions.h>
#include <utils/common/concurrent.h>

#include "ec2_thread_pool.h"
#include "database/db_manager.h"
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"
#include "transaction/transaction_message_bus.h"
#include <transaction/binary_transaction_serializer.h>
#include <api/app_server_connection.h>
#include <ec_connection_notification_manager.h>
#include "ec_connection_audit_manager.h"
#include "utils/common/threadqueue.h"

namespace ec2 {

namespace detail {

namespace aux {
template<typename Handler>
struct ScopeHandlerGuard
{
    const ErrorCode *ecode;
    Handler handler;

    ScopeHandlerGuard(const ErrorCode *ecode, Handler handler):
        ecode(ecode),
        handler(std::move(handler))
    {}

    ScopeHandlerGuard(const ScopeHandlerGuard<Handler>&) = delete;
    ScopeHandlerGuard<Handler>& operator=(const ScopeHandlerGuard<Handler>&) = delete;

    ScopeHandlerGuard(ScopeHandlerGuard<Handler>&&) = default;
    ScopeHandlerGuard<Handler>& operator=(ScopeHandlerGuard<Handler>&&) = default;

    ~ScopeHandlerGuard()
    {
        QnConcurrent::run(Ec2ThreadPool::instance(), std::bind(std::move(handler), *ecode));
    }
};

template<typename Handler>
ScopeHandlerGuard<Handler> createScopeHandlerGuard(ErrorCode& errorCode, Handler handler)
{
    return ScopeHandlerGuard<Handler>(&errorCode, handler);
}

struct AuditData
{
    ECConnectionAuditManager* auditManager;
    QnAuthSession authSession;
    Qn::UserAccessData userAccessData;

    AuditData(
        ECConnectionAuditManager* auditManager,
        const QnAuthSession& authSession,
        const Qn::UserAccessData& userAccessData)
        :
        auditManager(auditManager),
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

    QnAppServerConnectionFactory::getConnection2()
        ->notificationManager()
        ->triggerNotification(tran, NotificationSource::Local);
}
}

class ServerQueryProcessor;

struct PostProcessTransactionFunction
{
    template<class T>
    void operator()(const aux::AuditData& auditData, const QnTransaction<T>& tran) const;
};

class ServerQueryProcessor
{
public:

    typedef QnSafeQueue<std::function<void()>> PostProcessList;

    virtual ~ServerQueryProcessor() {}

    ServerQueryProcessor(const Qn::UserAccessData &userAccessData):
        m_userAccessData(userAccessData),
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
        using namespace std::placeholders;
        doAsyncExecuteTranCall(
            tran,
            handler,
            [this](
                QnTransaction<QueryDataType>& tran,
                PostProcessList* const transactionsPostProcessList) -> ErrorCode
            {
                return processUpdateSync(tran, transactionsPostProcessList);
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

        Qn::UserAccessData accessDataCopy(m_userAccessData);
        QnConcurrent::run(Ec2ThreadPool::instance(),
            [accessDataCopy, input, handler]()
            {
                OutputData output;
                const ErrorCode errorCode = dbManager(accessDataCopy).doQuery(input, output);
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

        Qn::UserAccessData accessDataCopy(m_userAccessData);
        QnConcurrent::run(Ec2ThreadPool::instance(),
            [accessDataCopy, input1, input2, handler]()
            {
                OutputData output;
                const ErrorCode errorCode = dbManager(accessDataCopy).doQuery(
                    input1, input2, output);
                handler(errorCode, output);
            });
    }

    void setAuditData(ECConnectionAuditManager* auditManager, const QnAuthSession& authSession);

private:
    static PostProcessList& getStaticPostProcessList();
    static QnMutex& getStaticUpdateMutex();

    aux::AuditData createAuditDataCopy()
    {
        return aux::AuditData(m_auditManager, m_authSession, m_userAccessData);
    }

    /**
     * @param syncFunction ErrorCode(QnTransaction<QueryDataType>&,
     *     PostProcessList*)
     */
    template<class QueryDataType, class CompletionHandlerType, class SyncFunctionType>
    void doAsyncExecuteTranCall(
        QnTransaction<QueryDataType>& tran,
        CompletionHandlerType completionHandler,
        SyncFunctionType syncFunction)
    {
        ErrorCode errorCode = ErrorCode::ok;
        auto scopeGuard = aux::createScopeHandlerGuard(errorCode, completionHandler);

        PostProcessList& transactionsPostProcessList = getStaticPostProcessList();
        QnMutex& updateDataMutex = getStaticUpdateMutex();
        // Starting transaction.
        {
            QnMutexLocker lock(&updateDataMutex);
            std::unique_ptr<detail::QnDbManager::QnDbTransactionLocker> dbTran;
            PostProcessList localPostProcessList;

            if (ApiCommand::isPersistent(tran.command))
            {
                dbTran.reset(new detail::QnDbManager::QnDbTransactionLocker(
                    dbManager(m_userAccessData).getTransaction()));
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
                if (!getTransactionDescriptorByTransaction(tran)->checkSavePermissionFunc(m_userAccessData, tran.params))
                {
                    errorCode = ErrorCode::forbidden;
                    return;
                }
                localPostProcessList.push(std::bind(PostProcessTransactionFunction(), createAuditDataCopy(), tran));
            }

            std::function<void()> postProcessAction;
            while (localPostProcessList.pop(postProcessAction, 0))
                transactionsPostProcessList.push(postProcessAction);
        }

        // Sending transactions.
        std::function<void()> postProcessAction;
        while (transactionsPostProcessList.pop(postProcessAction, 0))
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
        doAsyncExecuteTranCall(
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
            m_previousAccessRight(owner->m_userAccessData)
        {
            m_owner->m_userAccessData = accessRight;
        }

        ~AccessRightGrant()
        {
            m_owner->m_userAccessData = m_previousAccessRight;
        }
    private:
        ServerQueryProcessor* m_owner;
        Qn::UserAccessData m_previousAccessRight;
    };

    ErrorCode removeResourceSync(
        QnTransaction<ApiIdData>& tran,
        ApiObjectType resourceType,
        PostProcessList* const transactionsPostProcessList)
    {
        ErrorCode errorCode = removeResourceNestedObjectsSync(tran, resourceType, transactionsPostProcessList);
        if (errorCode != ErrorCode::ok)
            return errorCode;
        return processUpdateSync(tran, transactionsPostProcessList, 0);
    }

    ErrorCode removeResourceNestedObjectsSync(
        QnTransaction<ApiIdData>& tran,
        ApiObjectType resourceType,
        PostProcessList* const transactionsPostProcessList)
    {
        // Remove nested objects without checking actual access rights. Only main remove transaction will be validated.
        AccessRightGrant grant(this, Qn::kSystemAccess);

        ErrorCode errorCode = ErrorCode::ok;
        auto connection = QnAppServerConnectionFactory::getConnection2();

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
                        dbManager(m_userAccessData)
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
                        dbManager(m_userAccessData)
                            .getNestedObjectsNoLock(ApiObjectInfo(resourceType, tran.params.id))
                            .toIdList(),
                        transactionsPostProcessList),
                    lit("Remove user child resources failed"));


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
                        dbManager(m_userAccessData)
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

    ErrorCode processUpdateSync(
        QnTransaction<ApiResetBusinessRuleData>& tran,
        PostProcessList* const transactionsPostProcessList,
        int /*dummy*/ = 0)
    {
        ErrorCode errorCode = processMultiUpdateSync(
            ApiCommand::removeEventRule,
            tran.transactionType,
            dbManager(m_userAccessData).getObjectsNoLock(ApiObject_BusinessRule).toIdList(),
            transactionsPostProcessList);
        if(errorCode != ErrorCode::ok)
            return errorCode;

        return processMultiUpdateSync(
            ApiCommand::saveEventRule,
            tran.transactionType,
            tran.params.defaultRules,
            transactionsPostProcessList);
    }

    template<class QueryDataType>
    ErrorCode processUpdateSync(
        QnTransaction<QueryDataType>& tran,
        PostProcessList* const transactionsPostProcessList,
        int /*dummy*/ = 0)
    {
        NX_ASSERT(ApiCommand::isPersistent(tran.command));

        tran.transactionType = getTransactionDescriptorByTransaction(tran)->getTransactionTypeFunc(tran.params);
        if (tran.transactionType == TransactionType::Unknown)
            return ErrorCode::forbidden;

        transactionLog->fillPersistentInfo(tran);
        QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);

        ErrorCode errorCode =
            dbManager(m_userAccessData).executeTransactionNoLock(tran, serializedTran);
        NX_ASSERT(errorCode != ErrorCode::containsBecauseSequence
            && errorCode != ErrorCode::containsBecauseTimestamp);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        transactionsPostProcessList->push(std::bind(PostProcessTransactionFunction(), createAuditDataCopy(), tran));

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
            case ApiCommand::removeUserRole:
                return removeResourceSync(tran, ApiObjectUserRole, transactionsPostProcessList);
            case ApiCommand::removeResource:
            {
                QnTransaction<ApiIdData> updatedTran = tran;
                switch(dbManager(m_userAccessData).getObjectTypeNoLock(tran.params.id))
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
        using namespace std::placeholders;
        doAsyncExecuteTranCall(
            multiTran,
            handler,
            [this, subCommand](
                QnTransaction<QueryDataType>& multiTran,
                PostProcessList* const transactionsPostProcessList) -> ErrorCode
            {
                return processMultiUpdateSync(
                    subCommand, multiTran.transactionType, multiTran.params,
                    transactionsPostProcessList);
            });
    }


private:
    Qn::UserAccessData m_userAccessData;
    ECConnectionAuditManager* m_auditManager;
    QnAuthSession m_authSession;

    template<typename DataType>
    QnTransaction<DataType> createTransaction(
        ApiCommand::Value command,
        DataType data)
    {
        QnTransaction<DataType> transaction(command, std::move(data));
        transaction.historyAttributes.author = m_userAccessData.userId;
        return transaction;
    }
};

template<class T>
void PostProcessTransactionFunction::operator()(const aux::AuditData& auditData, const QnTransaction<T>& tran) const
{
    qnTransactionBus->sendTransaction(tran);
    aux::triggerNotification(auditData, tran);
}

} // namespace detail

struct ServerQueryProcessorAccess
{
    detail::ServerQueryProcessor getAccess(const Qn::UserAccessData userAccessData)
    {
        return detail::ServerQueryProcessor(userAccessData);
    }
};

} // namespace ec2
