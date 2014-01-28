/**********************************************************
* 27 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC_API_IMPL_H
#define EC_API_IMPL_H

#include <memory>

#include <QtCore/QObject.h>

#include <api/model/kvpair.h>
#include <business/business_fwd.h>
#include <core/resource/camera_history.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_type.h>
#include <licensing/license.h>


#define QT_API_IMPL

#ifdef QT_API_IMPL

//!Defining multiple handler macro because vs2012 does not support variadic templates. Will move to single macro after move to vs2013

#define DEFINE_ONE_ARG_HANDLER( REQUEST_NAME, FIRST_ARG_TYPE )                          \
    typedef OneParamHandler<FIRST_ARG_TYPE> REQUEST_NAME##Handler;                      \
    typedef std::shared_ptr<REQUEST_NAME##Handler> REQUEST_NAME##HandlerPtr;            \
    template<class TargetType, class HandlerType>                                                         \
        class Custom##REQUEST_NAME##Handler                                             \
    :                                                                                   \
        public AppServerSignaller,                                                      \
        public REQUEST_NAME##Handler                                                    \
    {                                                                                   \
    public:                                                                             \
        Custom##REQUEST_NAME##Handler( TargetType* target, HandlerType func, Qt::ConnectionType connectionType = Qt::AutoConnection ) {         \
            QObject::connect( static_cast<AppServerSignaller*>(this), &AppServerSignaller::on##REQUEST_NAME##Done, target, func, connectionType ); \
        }                                                                               \
        virtual void done( const FIRST_ARG_TYPE& val1 ) override { emit##REQUEST_NAME##Done( val1 ); }; \
    };


#define DEFINE_TWO_ARG_HANDLER( REQUEST_NAME, FIRST_ARG_TYPE, SECOND_ARG_TYPE )         \
    typedef TwoParamHandler<FIRST_ARG_TYPE, SECOND_ARG_TYPE> REQUEST_NAME##Handler;     \
    typedef std::shared_ptr<REQUEST_NAME##Handler> REQUEST_NAME##HandlerPtr;            \
    template<class TargetType, class HandlerType>                                                         \
        class Custom##REQUEST_NAME##Handler                                             \
    :                                                                                   \
        public AppServerSignaller,                                                      \
        public REQUEST_NAME##Handler                                                    \
    {                                                                                   \
    public:                                                                             \
        Custom##REQUEST_NAME##Handler( TargetType* target, HandlerType func, Qt::ConnectionType connectionType = Qt::AutoConnection ) {         \
            QObject::connect( static_cast<AppServerSignaller*>(this), &AppServerSignaller::on##REQUEST_NAME##Done, target, func, connectionType ); \
        } \
        virtual void done( \
            const FIRST_ARG_TYPE& val1, \
            const SECOND_ARG_TYPE& val2 ) override { emit##REQUEST_NAME##Done( val1, val2 ); }; \
    };


#define DEFINE_THREE_ARG_HANDLER( REQUEST_NAME, FIRST_ARG_TYPE, SECOND_ARG_TYPE, THIRD_ARG_TYPE )         \
    typedef ThreeParamHandler<FIRST_ARG_TYPE, SECOND_ARG_TYPE, THIRD_ARG_TYPE> REQUEST_NAME##Handler;     \
    typedef std::shared_ptr<REQUEST_NAME##Handler> REQUEST_NAME##HandlerPtr;            \
    template<class TargetType, class HandlerType>                                       \
        class Custom##REQUEST_NAME##Handler                                             \
    :                                                                                   \
        public AppServerSignaller,                                                      \
        public REQUEST_NAME##Handler                                                    \
    {                                                                                   \
    public:                                                                             \
        Custom##REQUEST_NAME##Handler( TargetType* target, HandlerType func, Qt::ConnectionType connectionType = Qt::AutoConnection ) {         \
            QObject::connect( static_cast<AppServerSignaller*>(this), &AppServerSignaller::on##REQUEST_NAME##Done, target, func, connectionType ); \
        } \
        virtual void done( \
            const FIRST_ARG_TYPE& val1, \
            const SECOND_ARG_TYPE& val2, \
            const THIRD_ARG_TYPE& val3 ) override { emit##REQUEST_NAME##Done( val1, val2, val3 ); }; \
    };

#else

#define DEFINE_ONE_ARG_HANDLER( REQUEST_NAME, FIRST_ARG_TYPE )         \
    typedef OneParamHandler<FIRST_ARG_TYPE> REQUEST_NAME##Handler;     \
    typedef std::shared_ptr<REQUEST_NAME##Handler> REQUEST_NAME##HandlerPtr;            \
    template<class HandlerType>                                                         \
        class Custom##REQUEST_NAME##Handler                                             \
    :                                                                                   \
        public CustomOneParamHandler<HandlerType, REQUEST_NAME##Handler>                \
    {                                                                                   \
    public:                                                                             \
        typedef CustomOneParamHandler<HandlerType, REQUEST_NAME##Handler> base_type;    \
        Custom##REQUEST_NAME##Handler( HandlerType func ) : base_type( func ) {}        \
    };

//!Using "two arg" handler because vs2012 does not support variadic templates. Will fix it after move to vs2013
#define DEFINE_TWO_ARG_HANDLER( REQUEST_NAME, FIRST_ARG_TYPE, SECOND_ARG_TYPE )         \
    typedef TwoParamHandler<FIRST_ARG_TYPE, SECOND_ARG_TYPE> REQUEST_NAME##Handler;     \
    typedef std::shared_ptr<REQUEST_NAME##Handler> REQUEST_NAME##HandlerPtr;            \
    template<class HandlerType>                                                         \
        class Custom##REQUEST_NAME##Handler                                             \
    :                                                                                   \
        public CustomTwoParamHandler<HandlerType, REQUEST_NAME##Handler>                \
    {                                                                                   \
    public:                                                                             \
        typedef CustomTwoParamHandler<HandlerType, REQUEST_NAME##Handler> base_type;    \
        Custom##REQUEST_NAME##Handler( HandlerType func ) : base_type( func ) {}        \
    };

#define DEFINE_THREE_ARG_HANDLER( REQUEST_NAME, FIRST_ARG_TYPE, SECOND_ARG_TYPE, THIRD_ARG_TYPE )         \
    typedef ThreeParamHandler<FIRST_ARG_TYPE, SECOND_ARG_TYPE, THIRD_ARG_TYPE> REQUEST_NAME##Handler;     \
    typedef std::shared_ptr<REQUEST_NAME##Handler> REQUEST_NAME##HandlerPtr;            \
    template<class HandlerType>                                                         \
        class Custom##REQUEST_NAME##Handler                                             \
    :                                                                                   \
        public CustomThreeParamHandler<HandlerType, REQUEST_NAME##Handler>                \
    {                                                                                   \
    public:                                                                             \
        typedef CustomThreeParamHandler<HandlerType, REQUEST_NAME##Handler> base_type;    \
        Custom##REQUEST_NAME##Handler( HandlerType func ) : base_type( func ) {}        \
    };

#endif


namespace ec2
{
    class AbstractECConnection;
    typedef std::shared_ptr<AbstractECConnection> AbstractECConnectionPtr;

    enum class ErrorCode
    {
        ok,
        failure
    };

    QString toString( ErrorCode errorCode );

    namespace impl
    {
        template<class Param1>
        class OneParamHandler
        {
        public:
            typedef Param1 first_type;

            virtual ~OneParamHandler() {}
            virtual void done( const Param1& val1 ) = 0;
        };

#ifndef QT_API_IMPL
        template<class Func, class BaseType>
        class CustomOneParamHandler
        :
            public BaseType
        {
        public:
            CustomOneParamHandler( const Func& func ) : m_func( func ) {}
            virtual void done( const typename BaseType::first_type& val1 ) override { m_func( val1 ); };
        private:
            Func m_func;
        };
#endif


        template<class Param1, class Param2>
        class TwoParamHandler
        {
        public:
            typedef Param1 first_type;
            typedef Param2 second_type;

            virtual ~TwoParamHandler() {}
            virtual void done( const Param1& val1, const Param2& val2 ) = 0;
        };

#ifndef QT_API_IMPL
        template<class Func, class BaseType>
        class CustomTwoParamHandler
        :
            public BaseType
        {
        public:
            CustomTwoParamHandler( const Func& func ) : m_func( func ) {}
            virtual void done( const typename BaseType::first_type& val1, const typename BaseType::second_type& val2 ) override { m_func( val1, val2 ); };
        private:
            Func m_func;
        };
#endif


        template<class Param1, class Param2, class Param3>
        class ThreeParamHandler
        {
        public:
            typedef Param1 first_type;
            typedef Param2 second_type;
            typedef Param3 third_type;

            virtual ~ThreeParamHandler() {}
            virtual void done( const Param1& val1, const Param2& val2, const Param3& val3 ) = 0;
        };

#ifndef QT_API_IMPL
        template<class Func, class BaseType>
        class CustomThreeParamHandler
        :
            public BaseType
        {
        public:
            CustomThreeParamHandler( const Func& func ) : m_func( func ) {}
            virtual void done(
                const typename BaseType::first_type& val1,
                const typename BaseType::second_type& val2,
                const typename BaseType::third_type& val3 ) override { m_func( val1, val2, val3 ); };
        private:
            Func m_func;
        };
#endif




#ifdef QT_API_IMPL
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
            void emitGetKvPairsDone( const ErrorCode p1, const QnKvPairListsById& p2 ) { emit onGetKvPairsDone( p1, p2 ); }
            void emitSaveServerDone( const ErrorCode p1, const QnMediaServerResourceList& p2, const QByteArray& p3 ) { emit onSaveServerDone( p1, p2, p3 ); }
            void emitGetServersDone( const ErrorCode p1, const QnMediaServerResourceList& p2 ) { emit onGetServersDone( p1, p2 ); }
            void emitAddCameraDone( const ErrorCode p1, const QnVirtualCameraResourceListPtr& p2 ) { emit onAddCameraDone( p1, p2 ); }
            void emitGetCamerasDone( const ErrorCode p1, const QnVirtualCameraResourceList& p2 ) { emit onGetCamerasDone( p1, p2 ); }
            void emitGetCamerasHistoryDone( const ErrorCode p1, const QnCameraHistoryList& p2 ) { emit onGetCamerasHistoryDone( p1, p2 ); }
            void emitGetUsersDone( const ErrorCode p1, const QnUserResourceList& p2 ) { emit onGetUsersDone( p1, p2 ); }
            void emitGetBusinessRulesDone( const ErrorCode p1, const QnBusinessEventRuleList& p2 ) { emit onGetBusinessRulesDone( p1, p2 ); }
            void emitGetLicensesDone( const ErrorCode p1, const QnLicenseList& p2 ) { emit onGetLicensesDone( p1, p2 ); }
            void emitGetLayoutsDone( const ErrorCode p1, const QnLayoutResourceList& p2 ) { emit onGetLayoutsDone( p1, p2 ); }
            void emitGetStoredFileDone( const ErrorCode p1, const QByteArray& p2 ) { emit onGetStoredFileDone( p1, p2 ); }
            void emitListDirectoryDone( const ErrorCode p1, const QStringList& p2 ) { emit onListDirectoryDone( p1, p2 ); }
            void emitCurrentTimeDone( const ErrorCode p1, const qint64& p2 ) { emit onCurrentTimeDone( p1, p2 ); }
            void emitDumpDatabaseDone( const ErrorCode p1, const QByteArray& p2 ) { emit onDumpDatabaseDone( p1, p2 ); }
            void emitGetSettingsDone( const ErrorCode p1, const QnKvPairList& p2 ) { emit onGetSettingsDone( p1, p2 ); }
            void emitConnectDone( const ErrorCode p1, AbstractECConnectionPtr p2 ) { emit onConnectDone( p1, p2 ); }
        
        signals:
            void onSimpleDone( const ErrorCode );
            void onGetResourceTypesDone( const ErrorCode, const QnResourceTypeList& );
            void onGetResourcesDone( const ErrorCode, const QnResourceList& );
            void onGetResourceDone( const ErrorCode, const QnResourcePtr& );
            void onGetKvPairsDone( const ErrorCode, const QnKvPairListsById& );
            void onSaveServerDone( const ErrorCode, const QnMediaServerResourceList&, const QByteArray& );
            void onGetServersDone( const ErrorCode, const QnMediaServerResourceList& );
            void onAddCameraDone( const ErrorCode, const QnVirtualCameraResourceListPtr& );
            void onGetCamerasDone( const ErrorCode, const QnVirtualCameraResourceList& );
            void onGetCamerasHistoryDone( const ErrorCode, const QnCameraHistoryList& );
            void onGetUsersDone( const ErrorCode, const QnUserResourceList& );
            void onGetBusinessRulesDone( const ErrorCode, const QnBusinessEventRuleList& );
            void onGetLicensesDone( const ErrorCode, const QnLicenseList& );
            void onGetLayoutsDone( const ErrorCode, const QnLayoutResourceList& );
            void onGetStoredFileDone( const ErrorCode, const QByteArray& );
            void onListDirectoryDone( const ErrorCode, const QStringList& );
            void onCurrentTimeDone( const ErrorCode, const qint64& );
            void onDumpDatabaseDone( const ErrorCode, const QByteArray& );
            void onGetSettingsDone( const ErrorCode, const QnKvPairList& );
            void onConnectDone( const ErrorCode, AbstractECConnectionPtr );
        };
#endif




        //////////////////////////////////////////////////////////
        ///////// Common handlers
        //////////////////////////////////////////////////////////
        DEFINE_ONE_ARG_HANDLER( Simple, ErrorCode )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractResourceManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetResourceTypes, ErrorCode, QnResourceTypeList )
        DEFINE_TWO_ARG_HANDLER( GetResources, ErrorCode, QnResourceList )
        DEFINE_TWO_ARG_HANDLER( GetResource, ErrorCode, QnResourcePtr )
        DEFINE_TWO_ARG_HANDLER( GetKvPairs, ErrorCode, QnKvPairListsById )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractMediaServerManager
        //////////////////////////////////////////////////////////
        DEFINE_THREE_ARG_HANDLER( SaveServer, ErrorCode, QnMediaServerResourceList, QByteArray )
        DEFINE_TWO_ARG_HANDLER( GetServers, ErrorCode, QnMediaServerResourceList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractCameraManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( AddCamera, ErrorCode, QnVirtualCameraResourceListPtr )
        DEFINE_TWO_ARG_HANDLER( GetCameras, ErrorCode, QnVirtualCameraResourceListPtr )
        DEFINE_TWO_ARG_HANDLER( GetCamerasHistory, ErrorCode, QnCameraHistoryListPtr )
        

        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractUserManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetUsers, ErrorCode, QnUserResourceList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractBusinessEventManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetBusinessRules, ErrorCode, QnBusinessEventRuleList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractLicenseManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetLicenses, ErrorCode, QnLicenseList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractLayoutManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetLayouts, ErrorCode, QnLayoutResourceList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractStoredFileManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetStoredFile, ErrorCode, QByteArray )
        DEFINE_TWO_ARG_HANDLER( ListDirectory, ErrorCode, QStringList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractECConnection
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( CurrentTime, ErrorCode, qint64 )
        DEFINE_TWO_ARG_HANDLER( DumpDatabase, ErrorCode, QByteArray )
        DEFINE_TWO_ARG_HANDLER( GetSettings, ErrorCode, QnKvPairList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractECConnectionFactory
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( Connect, ErrorCode, AbstractECConnectionPtr )
    }
}

#endif  //EC_API_IMPL_H
