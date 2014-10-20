
#ifndef EC_QT_API_H
#define EC_QT_API_H

#include <functional>

#include <QtCore/QObject>

#include "ec_api.h"
#include "impl/ec_api_impl.h"


namespace ec2
{
    /*!
        Contains types to enable usage of ec_api in the following way:

        \code{.cpp}
        CustomQObject someObject; //SomeQObject inherits CustomQObject
        ec2::AbstractResourceManager* rm;
        rm->getResourceTypes( ec2::qt::bind_slot( &someObject, &CustomQObject::some_slot ) );
        rm->getResourceTypes( ec2::qt::bind_slot( &someObject, []{} ) );
        \endcode

        Unfortunately, Qt moc does support neither templates nor macro. So, have to declare separate handler class for each request
    */
    namespace qt
    {
        class AppServerSignaller
        :
            public QObject
        {
            Q_OBJECT

        public:
            void emitSimpleDone( const ErrorCode p1 ) { emit onSimpleDone( p1 ); }
            void emitGetResourceTypesDone( const ErrorCode p1, const QnResourceTypeList& p2 ) { emit onGetResourceTypesDone( p1, p2 ); }
            void emitGetResourcesDone( const ErrorCode p1, const QnResourceList& p2 ) { emit onGetResourcesDone( p1, p2 ); }
            void emitGetResourceDone( const ErrorCode p1, const QnResourcePtr& p2 ) { emit onGetResourceDone( p1, p2 ); }
            void emitGetKvPairsDone( const ErrorCode p1, const ApiResourceParamWithRefDataList& p2 ) { emit onGetKvPairsDone( p1, p2 ); }
            void emitSaveServerDone( const ErrorCode p1, const QnMediaServerResourceList& p2) { emit onSaveServerDone( p1, p2); }
            void emitGetServersDone( const ErrorCode p1, const QnMediaServerResourceList& p2 ) { emit onGetServersDone( p1, p2 ); }
            void emitAddCameraDone( const ErrorCode p1, const QnVirtualCameraResourceList& p2 ) { emit onAddCameraDone( p1, p2 ); }
            void emitGetCamerasDone( const ErrorCode p1, const QnVirtualCameraResourceList& p2 ) { emit onGetCamerasDone( p1, p2 ); }
            void emitGetCamerasHistoryDone( const ErrorCode p1, const QnCameraHistoryList& p2 ) { emit onGetCamerasHistoryDone( p1, p2 ); }
            void emitGetCameraBookmarkTagsDone( const ErrorCode p1, const QnCameraBookmarkTags& p2 ) { emit onGetCameraBookmarkTagsDone( p1, p2 ); }
            void emitGetUsersDone( const ErrorCode p1, const QnUserResourceList& p2 ) { emit onGetUsersDone( p1, p2 ); }
            void emitGetBusinessRulesDone( const ErrorCode p1, const QnBusinessEventRuleList& p2 ) { emit onGetBusinessRulesDone( p1, p2 ); }
            void emitGetLicensesDone( const ErrorCode p1, const QnLicenseList& p2 ) { emit onGetLicensesDone( p1, p2 ); }
            void emitGetLayoutsDone( const ErrorCode p1, const QnLayoutResourceList& p2 ) { emit onGetLayoutsDone( p1, p2 ); }
            void emitGetStoredFileDone( const ErrorCode p1, const QByteArray& p2 ) { emit onGetStoredFileDone( p1, p2 ); }
            void emitListDirectoryDone( const ErrorCode p1, const QStringList& p2 ) { emit onListDirectoryDone( p1, p2 ); }
            void emitCurrentTimeDone( const ErrorCode p1, const qint64& p2 ) { emit onCurrentTimeDone( p1, p2 ); }
            void emitDumpDatabaseDone( const ErrorCode p1, const ec2::ApiDatabaseDumpData& p2 ) { emit onDumpDatabaseDone( p1, p2 ); }
            void emitConnectDone( const ErrorCode p1, const AbstractECConnection* p2 ) { emit onConnectDone( p1, p2 ); }
        
        signals:
            void onSimpleDone( const ErrorCode );
            void onGetResourceTypesDone( const ErrorCode, const QnResourceTypeList& );
            void onGetResourcesDone( const ErrorCode, const QnResourceList& );
            void onGetResourceDone( const ErrorCode, const QnResourcePtr& );
            void onGetKvPairsDone( const ErrorCode, const ApiResourceParamWithRefDataList& );
            void onSaveServerDone( const ErrorCode, const QnMediaServerResourceList&);
            void onGetServersDone( const ErrorCode, const QnMediaServerResourceList& );
            void onAddCameraDone( const ErrorCode, const QnVirtualCameraResourceList& );
            void onGetCamerasDone( const ErrorCode, const QnVirtualCameraResourceList& );
            void onGetCamerasHistoryDone( const ErrorCode, const QnCameraHistoryList& );
            void onGetCameraBookmarkTagsDone(const ErrorCode, const QnCameraBookmarkTags&);
            void onGetUsersDone( const ErrorCode, const QnUserResourceList& );
            void onGetBusinessRulesDone( const ErrorCode, const QnBusinessEventRuleList& );
            void onGetLicensesDone( const ErrorCode, const QnLicenseList& );
            void onGetLayoutsDone( const ErrorCode, const QnLayoutResourceList& );
            void onGetStoredFileDone( const ErrorCode, const QByteArray& );
            void onListDirectoryDone( const ErrorCode, const QStringList& );
            void onCurrentTimeDone( const ErrorCode, const qint64& );
            void onDumpDatabaseDone( const ErrorCode, const ec2::ApiDatabaseDumpData& );
            void onConnectDone( const ErrorCode, const AbstractECConnection* );
        };

        template<class T, class P1>
        class SlotBinder1
        {
        public:
            template<class Func>
                SlotBinder1( T* obj, Func _f )
            :
                m_signaller( std::make_shared<AppServerSignaller>() )
            {
                QObject::connect( m_signaller.get(), &AppServerSignaller::onGetResourceTypesDone, obj, _f );
            }

            void operator()( ec2::ErrorCode errorCode, const P1& val1 ) { m_signaller->emitGetResourceTypesDone( errorCode, val1 ); }

        private:
            std::shared_ptr<AppServerSignaller> m_signaller;
        };

        template<class T>
            SlotBinder1<T, QnResourceTypeList> bind_slot( T* obj, void (T::*s_ptr)(ec2::ErrorCode, const QnResourceTypeList&) )
        {
            return SlotBinder1<T, QnResourceTypeList>( obj, s_ptr );
        }

        template<class T>
            SlotBinder1<T, QnResourceTypeList> bind_slot( T* obj, std::function<void(ec2::ErrorCode, const QnResourceTypeList&)> _f )
        {
            return SlotBinder1<T, QnResourceTypeList>( obj, _f );
        }
    }
}

#endif  //EC_QT_API_H
