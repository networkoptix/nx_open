#pragma once

#include <memory>
#include <map>

#include <common/common_module_aware.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/thread/joinable.h>
#include <nx/utils/thread/stoppable.h>
#include <nx_ec/ec_api.h>

#include "ec2_connection.h"
#include "settings.h"

namespace ec2 {

	struct ServerQueryProcessorAccess;
    class QnDistributedMutexManager;

	// TODO: #2.4 remove Ec2 prefix to avoid ec2::LocalConnectionFactory
	class LocalConnectionFactory:
		public AbstractECConnectionFactory,
		public QnStoppable,
		public QnJoinable
	{
	public:
		LocalConnectionFactory(
            QnCommonModule* commonModule,
            Qn::PeerType peerType,
            nx::utils::TimerManager* const timerManager,
			bool isP2pMode);
		virtual ~LocalConnectionFactory();

		virtual void pleaseStop() override;
		virtual void join() override;

		/**
		* Implementation of AbstractECConnectionFactory::testConnectionAsync.
		*/
		virtual int testConnectionAsync(
			const nx::utils::Url& addr,
			impl::TestConnectionHandlerPtr handler) override;

		/**
		* Implementation of AbstractECConnectionFactory::connectAsync.
		*/
		virtual int connectAsync(
			const nx::utils::Url& addr,
			const ApiClientInfoData& clientInfo,
			impl::ConnectHandlerPtr handler) override;

		void registerRestHandlers(QnRestProcessorPool* const restProcessorPool);

		virtual void registerTransactionListener(
			QnHttpConnectionListener* httpConnectionListener);

		virtual void setConfParams(std::map<QString, QVariant> confParams) override;

		TransactionMessageBusAdapter* messageBus() const;
		QnDistributedMutexManager* distributedMutex() const;
		virtual TimeSynchronizationManager* timeSyncManager() const override;

		QnJsonTransactionSerializer* jsonTranSerializer() const;
		QnUbjsonTransactionSerializer* ubjsonTranSerializer() const;
		virtual void shutdown() override;

	private:
		QnMutex m_mutex;
		Settings m_settingsInstance;

		std::unique_ptr<QnJsonTransactionSerializer> m_jsonTranSerializer;
		std::unique_ptr<QnUbjsonTransactionSerializer> m_ubjsonTranSerializer;

		std::unique_ptr<detail::QnDbManager> m_dbManager;
		std::unique_ptr<QnTransactionLog> m_transactionLog;
		std::unique_ptr<TransactionMessageBusAdapter> m_bus;

		std::shared_ptr<ServerQueryProcessorAccess> m_serverQueryProcessor;
		std::unique_ptr<TimeSynchronizationManager> m_timeSynchronizationManager;
		std::unique_ptr<QnDistributedMutexManager> m_distributedMutexManager;
		bool m_terminated;
		int m_runningRequests;
		bool m_sslEnabled;

		Ec2DirectConnectionPtr m_directConnection;
		bool m_p2pMode = false;
	private:
		int establishDirectConnection(const nx::utils::Url& url, impl::ConnectHandlerPtr handler);

		void tryConnectToOldEC(const nx::utils::Url& ecUrl, impl::ConnectHandlerPtr handler, int reqId);

		template<class Handler>
		void connectToOldEC(const nx::utils::Url& ecURL, Handler completionFunc);

		/**
		* Called on server side to handle connection request from remote host.
		*/
		ErrorCode fillConnectionInfo(
			const ApiLoginData& loginInfo,
			QnConnectionInfo* const connectionInfo,
			nx::network::http::Response* response = nullptr);

		int testDirectConnection(const nx::utils::Url& addr, impl::TestConnectionHandlerPtr handler);
	    ErrorCode getSettings(
	        nullptr_t,
	        nx::vms::api::ResourceParamDataList* const outData,
	        const Qn::UserAccessData&);

		template<class InputDataType>
		void regUpdate(QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd,
			Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

		template<class InputDataType, class CustomActionType>
		void regUpdate(QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd,
			CustomActionType customAction, Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

		template<class InputDataType, class OutputDataType>
		void regGet(QnRestProcessorPool* const restProcessorPool, ApiCommand::Value cmd,
			Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

		template<class InputType, class OutputType>
		void regFunctor(
			QnRestProcessorPool* const restProcessorPool,
			ApiCommand::Value cmd,
			std::function<ErrorCode(InputType, OutputType*, const Qn::UserAccessData&)> handler,
			Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

		template<class InputType, class OutputType>
		void regFunctorWithResponse(
			QnRestProcessorPool* const restProcessorPool,
			ApiCommand::Value cmd,
			std::function<ErrorCode(InputType, OutputType*, nx::network::http::Response*)> handler,
			Qn::GlobalPermission permission = Qn::NoGlobalPermissions);

		template<class Function>
		void statisticsCall(const Function& function);
	};

} // namespace ec2
