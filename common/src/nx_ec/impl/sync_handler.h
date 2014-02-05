/**********************************************************
* 27 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef SYNC_HANDLER_H
#define SYNC_HANDLER_H

#include <condition_variable>
#include <mutex>

#include "ec_api_impl.h"


namespace ec2
{
    namespace impl
    {
        //!Allows executing ec api methods synchronously
        class SyncHandler
        {
        public:
            SyncHandler();

            void wait();
            ErrorCode errorCode() const;
            void done( int reqID, ErrorCode errorCode );

        private:
            std::condition_variable m_cond;
            mutable std::mutex m_mutex;
            bool m_done;
            ErrorCode m_errorCode;
        };

        template<class BaseHandler, class OutDataType>
        class CustomSyncHandler
        :
            public SyncHandler,
            public BaseHandler
        {
        public:
            CustomSyncHandler( OutDataType* const outParam )
                : m_outParam( outParam ) {}

            virtual void done( int reqID, const ErrorCode& errorCode, const OutDataType& _outParam ) override
            {
                if (m_outParam)
                    *m_outParam = _outParam;
                SyncHandler::done( reqID, errorCode );
            }

        private:
            OutDataType* const m_outParam;
        };


        template<class HandlerType, class OutParamType, class AsyncFuncType>
        ErrorCode doSyncCall( AsyncFuncType asyncFunc, OutParamType* outParam )
        {
            auto syncHandler = std::make_shared<impl::CustomSyncHandler<HandlerType, OutParamType>>( outParam );
            asyncFunc( syncHandler );
            syncHandler->wait();
            return syncHandler->errorCode();
        }




        template<class BaseHandler>
        class CustomSyncHandler1
            :
            public SyncHandler,
            public BaseHandler
        {
        public:
            virtual void done( int reqID, const ErrorCode& errorCode ) override
            {
                SyncHandler::done( reqID, errorCode );
            }
        };


        template<class HandlerType, class AsyncFuncType>
        ErrorCode doSyncCall( AsyncFuncType asyncFunc )
        {
            auto syncHandler = std::make_shared<impl::CustomSyncHandler1<HandlerType>>();
            asyncFunc( syncHandler );
            syncHandler->wait();
            return syncHandler->errorCode();
        }
    }
}

#endif  //SYNC_HANDLER_H
