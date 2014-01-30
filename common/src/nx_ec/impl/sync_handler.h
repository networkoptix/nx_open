/**********************************************************
* 27 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef SYNC_HANDLER_H
#define SYNC_HANDLER_H

#include <condition_variable>
#include <mutex>

#include <QtCore/QObject>

#include "ec_api_impl.h"


namespace ec2
{
    namespace impl
    {
        //!Allows executing ec api methods synchronously
        class SyncHandler
        :
            public QObject
        {
            Q_OBJECT

        public:
            SyncHandler();

            void wait();
            ErrorCode errorCode() const;

        public slots:
            void done( ErrorCode errorCode );

        private:
            std::condition_variable m_cond;
            mutable std::mutex m_mutex;
            bool m_done;
            ErrorCode m_errorCode;
        };

        class AddCameraSyncHandler
        :
            public SyncHandler,
            public AddCameraHandler
        {
        public:
            QnVirtualCameraResourceListPtr cameraList() const {
                return m_cameras;
            }

            virtual void done( const ErrorCode& errorCode, const QnVirtualCameraResourceListPtr& cameras ) override {
                m_cameras = cameras;
                SyncHandler::done( errorCode );
            }

        private:
            QnVirtualCameraResourceListPtr m_cameras;
        };

        class SyncConnectHandler
        :
            public SyncHandler,
            public ConnectHandler
        {
        public:
            AbstractECConnectionPtr connection() const {
                return m_connection;
            }

            virtual void done( const ErrorCode& errorCode, const AbstractECConnectionPtr& connection ) override {
                m_connection = connection;
                SyncHandler::done( errorCode );
            }

        private:
            AbstractECConnectionPtr m_connection;
        };

        class GetResourceTypesSyncHandler
        :
            public SyncHandler,
            public GetResourceTypesHandler
        {
        public:
            const QnResourceTypeList& resourceTypeList() const {
                return m_resourceTypeList;
            }

            virtual void done( const ErrorCode& errorCode, const QnResourceTypeList& resTypeList ) override {
                m_resourceTypeList = resTypeList;
                SyncHandler::done( errorCode );
            }

        private:
            QnResourceTypeList m_resourceTypeList;
        };

        class GetCamerasSyncHandler
            :
            public SyncHandler,
            public GetCamerasHandler
        {
        public:
            const QnVirtualCameraResourceList& cameraList() const {
                return m_cameraList;
            }

            virtual void done( const ErrorCode& errorCode, const QnVirtualCameraResourceList& cameras) override {
                m_cameraList = cameras;
                SyncHandler::done( errorCode );
            }

        private:
            QnVirtualCameraResourceList m_cameraList;
        };

    }
}

#endif  //SYNC_HANDLER_H
