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

namespace ec2 {

namespace detail {

struct SendTransactionFunction
{
    template<class T>
    void operator()(const QnTransaction<T>& tran) const
    {
        // Local transactions (such as setStatus for servers) should only be sent to clients.
        if (tran.isLocal)
        {
            QnPeerSet clients = qnTransactionBus->aliveClientPeers().keys().toSet();
            // Important check. Empty target means "send to all peers".
            if (!clients.isEmpty())
                qnTransactionBus->sendTransaction(tran, clients);
        }
        else
        {
            // Send transaction to all peers.
            qnTransactionBus->sendTransaction(tran);
        }
    }
};

class ServerQueryProcessor
{
public:
    virtual ~ServerQueryProcessor() {}

    ServerQueryProcessor(const Qn::UserAccessData &userAccessData):
        m_userAccessData(userAccessData)
    {
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
                std::list<std::function<void()>>* const transactionsToSend) -> ErrorCode
            {
                return processUpdateSync(tran, transactionsToSend);
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
        NX_ASSERT(tran.command == ApiCommand::saveServerUserAttributesList);
        return processMultiUpdateAsync<
            ApiMediaServerUserAttributesDataList, ApiMediaServerUserAttributesData>(
                tran, handler, ApiCommand::saveServerUserAttributes);
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

private:
    /**
     * @param syncFunction ErrorCode(QnTransaction<QueryDataType>&,
     *     std::list<std::function<void()>>*)
     */
    template<class QueryDataType, class CompletionHandlerType, class SyncFunctionType>
    void doAsyncExecuteTranCall(
        QnTransaction<QueryDataType>& tran,
        CompletionHandlerType completionHandler,
        SyncFunctionType syncFunction)
    {
        ErrorCode errorCode = ErrorCode::ok;
        auto SCOPED_GUARD_FUNC =
            [&errorCode, &completionHandler](ServerQueryProcessor*)
            {
                QnConcurrent::run(
                    Ec2ThreadPool::instance(), std::bind(completionHandler, errorCode));
            };
        std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD(
            this, SCOPED_GUARD_FUNC);

        QnMutexLocker lock(&m_updateDataMutex);

        // Starting transaction.
        std::unique_ptr<detail::QnDbManager::QnDbTransactionLocker> dbTran;
        std::list<std::function<void()>> transactionsToSend;

        if(ApiCommand::isPersistent(tran.command))
        {
            dbTran.reset(new detail::QnDbManager::QnDbTransactionLocker(
                dbManager(m_userAccessData).getTransaction()));
            errorCode = syncFunction(tran, &transactionsToSend);
            if(errorCode != ErrorCode::ok)
                return;
            if (!dbTran->commit())
            {
                errorCode = ErrorCode::dbError;
                return;
            }
        }
        else
        {
            transactionsToSend.push_back(std::bind(SendTransactionFunction(), tran));
        }

        // Sending transactions.
        for(auto& sendCommand: transactionsToSend)
            sendCommand();

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

    ErrorCode removeObjAttrHelper(
        const QnUuid& id,
        ApiCommand::Value command,
        const AbstractECConnectionPtr& connection,
        std::list<std::function<void()>>* const transactionsToSend);

    ErrorCode removeObjParamsHelper(
        const QnTransaction<ApiIdData>& tran,
        const AbstractECConnectionPtr& connection,
        std::list<std::function<void()>>* const transactionsToSend);

    ErrorCode removeResourceSync(
        QnTransaction<ApiIdData>& tran,
        ApiObjectType resourceType,
        std::list<std::function<void()>>* const transactionsToSend)
    {
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
                        connection,
                        transactionsToSend),
                    lit("Remove camera attributes failed"));

                RUN_AND_CHECK_ERROR(
                    removeObjParamsHelper(tran, connection, transactionsToSend),
                    lit("Remove camera params failed"));
                break;
            }

            case ApiObject_Server:
            {
                RUN_AND_CHECK_ERROR(
                    removeObjAttrHelper(
                        tran.params.id,
                        ApiCommand::removeServerUserAttributes,
                        connection,
                        transactionsToSend),
                    lit("Remove server attrs failed"));

                RUN_AND_CHECK_ERROR(
                    removeObjParamsHelper(tran, connection, transactionsToSend),
                    lit("Remove server params failed"));

                RUN_AND_CHECK_ERROR(
                    processMultiUpdateSync(
                        ApiCommand::removeResource,
                        tran.isLocal,
                        dbManager(m_userAccessData)
                            .getNestedObjectsNoLock(ApiObjectInfo(resourceType, tran.params.id))
                            .toIdList(),
                        transactionsToSend),
                    lit("Remove server child resources failed"));

                break;
            }

            case ApiObject_User:
            {
                RUN_AND_CHECK_ERROR(
                    removeObjParamsHelper(tran, connection, transactionsToSend),
                    lit("Remove user params failed"));
                break;
            }

            default:
                NX_ASSERT(0);
        }

        if(errorCode != ErrorCode::ok)
            return errorCode;

        #undef RUN_AND_CHECK_ERROR

        return processUpdateSync(tran, transactionsToSend, 0);
    }

    ErrorCode processUpdateSync(
        QnTransaction<ApiResetBusinessRuleData>& tran,
        std::list<std::function<void()>>* const transactionsToSend,
        int /*dummy*/ = 0)
    {
        ErrorCode errorCode = processMultiUpdateSync(
            ApiCommand::removeBusinessRule,
            tran.isLocal,
            dbManager(m_userAccessData).getObjectsNoLock(ApiObject_BusinessRule).toIdList(),
            transactionsToSend);
        if(errorCode != ErrorCode::ok)
            return errorCode;

        return processMultiUpdateSync(
            ApiCommand::saveBusinessRule,
            tran.isLocal,
            tran.params.defaultRules,
            transactionsToSend);
    }

    template<class QueryDataType>
    ErrorCode processUpdateSync(
        QnTransaction<QueryDataType>& tran,
        std::list<std::function<void()>>* const transactionsToSend,
        int /*dummy*/ = 0)
    {
        NX_ASSERT(ApiCommand::isPersistent(tran.command));

        transactionLog->fillPersistentInfo(tran);
        QByteArray serializedTran =
            QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
        ErrorCode errorCode =
            dbManager(m_userAccessData).executeTransactionNoLock(tran, serializedTran);
        NX_ASSERT(errorCode != ErrorCode::containsBecauseSequence
            && errorCode != ErrorCode::containsBecauseTimestamp);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        QnAppServerConnectionFactory::getConnection2()
            ->notificationManager()
            ->triggerNotification(tran);

        transactionsToSend->push_back(std::bind(SendTransactionFunction(), tran));

        return errorCode;
    }

    ErrorCode processUpdateSync(
        QnTransaction<ApiIdData>& tran,
        std::list<std::function<void()>>* const transactionsToSend)
    {
        switch (tran.command)
        {
            case ApiCommand::removeMediaServer:
                return removeResourceSync(tran, ApiObject_Server, transactionsToSend);
            case ApiCommand::removeUser:
                return removeResourceSync(tran, ApiObject_User, transactionsToSend);
            case ApiCommand::removeCamera:
                return removeResourceSync(tran, ApiObject_Camera, transactionsToSend);
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
                        updatedTran.command = ApiCommand::removeBusinessRule;
                        break;
                    default:
                        return processUpdateSync(tran, transactionsToSend, 0);
                }
                return processUpdateSync(updatedTran, transactionsToSend); //< calling recursively
            }
            default:
                return processUpdateSync(tran, transactionsToSend, 0);
        }
    }

    template<class SubDataType>
    ErrorCode processMultiUpdateSync(
        ApiCommand::Value command,
        bool isLocal,
        const std::vector<SubDataType>& nestedList,
        std::list<std::function<void()>>* const transactionsToSend)
    {
        for(const SubDataType& data: nestedList)
        {
            QnTransaction<SubDataType> subTran(command, data);
            subTran.isLocal = isLocal;
            ErrorCode errorCode = processUpdateSync(subTran, transactionsToSend);
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
                std::list<std::function<void()>>* const transactionsToSend) -> ErrorCode
            {
                return processMultiUpdateSync(
                    subCommand, multiTran.isLocal, multiTran.params,
                    transactionsToSend);
            });
    }

private:
    static QnMutex m_updateDataMutex;
    Qn::UserAccessData m_userAccessData;
};

} // namespace detail

struct ServerQueryProcessorAccess
{
    detail::ServerQueryProcessor getAccess(const Qn::UserAccessData userAccessData)
    {
        return detail::ServerQueryProcessor(userAccessData);
    }
};

} // namespace ec2
