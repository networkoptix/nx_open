/**********************************************************
* 27 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC_API_IMPL_H
#define EC_API_IMPL_H

#include <memory>

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <api/model/connection_info.h>
#include <api/model/kvpair.h>
#include <business/business_fwd.h>
#include <core/resource/camera_history.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_type.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <licensing/license.h>


//!Defining multiple handler macro because vs2012 does not support variadic templates. Will move to single variadic template after move to vs2013

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
        virtual void done( int reqID, const FIRST_ARG_TYPE& val1 ) override { emit##REQUEST_NAME##Done( reqID, val1 ); }; \
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
            int reqID, \
            const FIRST_ARG_TYPE& val1, \
            const SECOND_ARG_TYPE& val2 ) override { emit##REQUEST_NAME##Done( reqID, val1, val2 ); }; \
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
            int reqID, \
            const FIRST_ARG_TYPE& val1, \
            const SECOND_ARG_TYPE& val2, \
            const THIRD_ARG_TYPE& val3 ) override { emit##REQUEST_NAME##Done( reqID, val1, val2, val3 ); }; \
    };


namespace ec2
{
    const int INVALID_REQ_ID = -1;  

    // TODO: #Elric #enum
    enum class ErrorCode
    {
        ok,
        failure,
        ioError,
        serverError,
        unsupported,
        unauthorized,
        //!Response parse error
        badResponse,
        //!Error executing DB request
        dbError,
        containsBecauseTimestamp, // transaction already in database
        containsBecauseSequence,  // transaction already in database
        //!Method is not implemented yet
        notImplemented
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
            virtual void done( int reqID, const Param1& val1 ) = 0;
        };


        template<class Param1, class Param2>
        class TwoParamHandler
        {
        public:
            typedef Param1 first_type;
            typedef Param2 second_type;

            virtual ~TwoParamHandler() {}
            virtual void done( int reqID, const Param1& val1, const Param2& val2 ) = 0;
        };


        template<class Param1, class Param2, class Param3>
        class ThreeParamHandler
        {
        public:
            typedef Param1 first_type;
            typedef Param2 second_type;
            typedef Param3 third_type;

            virtual ~ThreeParamHandler() {}
            virtual void done( int reqID, const Param1& val1, const Param2& val2, const Param3& val3 ) = 0;
        };


        class AppServerSignaller
        :
            public QObject
        {
            Q_OBJECT

        public:
            void emitSimpleDone( int reqID, const ErrorCode p1 ) { emit onSimpleDone( reqID, p1 ); }
            void emitGetResourceTypesDone( int reqID, const ErrorCode p1, const QnResourceTypeList& p2 ) { emit onGetResourceTypesDone( reqID, p1, p2 ); }
            void emitSetResourceStatusDone( int reqID, const ErrorCode p1, const QUuid& p2 ) { emit onSetResourceStatusDone( reqID, p1, p2 ); }
            void emitSaveResourceDone( int reqID, const ErrorCode p1, const QnResourcePtr& p2 ) { emit onSaveResourceDone( reqID, p1, p2 ); }
            //void emitSetResourceDisabledDone( int reqID, const ErrorCode p1, const QUuid& p2 ) { emit onSetResourceDisabledDone( reqID, p1, p2 ); }
            void emitGetResourcesDone( int reqID, const ErrorCode p1, const QnResourceList& p2 ) { emit onGetResourcesDone( reqID, p1, p2 ); }
            void emitGetResourceDone( int reqID, const ErrorCode p1, const QnResourcePtr& p2 ) { emit onGetResourceDone( reqID, p1, p2 ); }
            void emitGetKvPairsDone( int reqID, const ErrorCode p1, const QnKvPairListsById& p2 ) { emit onGetKvPairsDone( reqID, p1, p2 ); }
            void emitSaveKvPairsDone( int reqID, const ErrorCode p1, const QnKvPairListsById& p2 ) { emit onSaveKvPairsDone( reqID, p1, p2 ); }
            void emitSaveServerDone( int reqID, const ErrorCode p1, const QnMediaServerResourcePtr& p2) { emit onSaveServerDone( reqID, p1, p2 ); }
            void emitSaveBusinessRuleDone( int reqID, const ErrorCode p1, const QnBusinessEventRulePtr& p2) { emit onSaveBusinessRuleDone( reqID, p1, p2 ); }
            void emitGetServersDone( int reqID, const ErrorCode p1, const QnMediaServerResourceList& p2 ) { emit onGetServersDone( reqID, p1, p2 ); }
            void emitAddCameraDone( int reqID, const ErrorCode p1, const QnVirtualCameraResourceList& p2 ) { emit onAddCameraDone( reqID, p1, p2 ); }
            void emitAddUserDone( int reqID, const ErrorCode p1, const QnUserResourceList& p2 ) { emit onAddUserDone( reqID, p1, p2 ); }
            void emitGetCamerasDone( int reqID, const ErrorCode p1, const QnVirtualCameraResourceList& p2 ) { emit onGetCamerasDone( reqID, p1, p2 ); }
            void emitGetCamerasHistoryDone( int reqID, const ErrorCode p1, const QnCameraHistoryList& p2 ) { emit onGetCamerasHistoryDone( reqID, p1, p2 ); }
            void emitGetCameraBookmarkTagsDone( int reqID, const ErrorCode p1, const QnCameraBookmarkTags& p2 ) { emit onGetCameraBookmarkTagsDone( reqID, p1, p2 ); }
            void emitGetUsersDone( int reqID, const ErrorCode p1, const QnUserResourceList& p2 ) { emit onGetUsersDone( reqID, p1, p2 ); }
            void emitGetBusinessRulesDone( int reqID, const ErrorCode p1, const QnBusinessEventRuleList& p2 ) { emit onGetBusinessRulesDone( reqID, p1, p2 ); }
            void emitGetLicensesDone( int reqID, const ErrorCode p1, const QnLicenseList& p2 ) { emit onGetLicensesDone( reqID, p1, p2 ); }
            void emitGetLayoutsDone( int reqID, const ErrorCode p1, const QnLayoutResourceList& p2 ) { emit onGetLayoutsDone( reqID, p1, p2 ); }
            void emitGetStoredFileDone( int reqID, const ErrorCode p1, const QByteArray& p2 ) { emit onGetStoredFileDone( reqID, p1, p2 ); }
            void emitListDirectoryDone( int reqID, const ErrorCode p1, const QStringList& p2 ) { emit onListDirectoryDone( reqID, p1, p2 ); }
            void emitCurrentTimeDone( int reqID, const ErrorCode p1, const qint64& p2 ) { emit onCurrentTimeDone( reqID, p1, p2 ); }
            void emitDumpDatabaseDone( int reqID, const ErrorCode p1, const ec2::ApiDatabaseDumpData& p2 ) { emit onDumpDatabaseDone( reqID, p1, p2 ); }
            void emitTestConnectionDone( int reqID, const ErrorCode p1, const QnConnectionInfo& p2 ) { emit onTestConnectionDone( reqID, p1, p2 ); }
            void emitConnectDone( int reqID, const ErrorCode p1, const AbstractECConnectionPtr &p2 ) { emit onConnectDone( reqID, p1, p2 ); }
            void emitAddVideowallDone( int reqID, const ErrorCode p1, const QnVideoWallResourceList& p2 ) { emit onAddVideowallDone( reqID, p1, p2 ); }
            void emitGetVideowallsDone( int reqID, const ErrorCode p1, const QnVideoWallResourceList& p2 ) { emit onGetVideowallsDone( reqID, p1, p2 ); }
        
        signals:
            void onSimpleDone( int reqID, const ErrorCode );
            void onGetResourceTypesDone( int reqID, const ErrorCode, const QnResourceTypeList& );
            void onSetResourceStatusDone( int reqID, const ErrorCode, const QUuid& );
            void onSaveResourceDone( int reqID, const ErrorCode, const QnResourcePtr& );
            //void onSetResourceDisabledDone( int reqID, const ErrorCode, const QUuid& );
            void onGetResourcesDone( int reqID, const ErrorCode, const QnResourceList& );
            void onGetResourceDone( int reqID, const ErrorCode, const QnResourcePtr& );
            void onGetKvPairsDone( int reqID, const ErrorCode, const QnKvPairListsById& );
            void onSaveKvPairsDone( int reqID, const ErrorCode, const QnKvPairListsById& );
            void onSaveServerDone( int reqID, const ErrorCode, const QnMediaServerResourcePtr&);
            void onSaveBusinessRuleDone( int reqID, const ErrorCode, const QnBusinessEventRulePtr&);
            void onGetServersDone( int reqID, const ErrorCode, const QnMediaServerResourceList& );
            void onAddCameraDone( int reqID, const ErrorCode, const QnVirtualCameraResourceList& );
            void onAddUserDone( int reqID, const ErrorCode, const QnUserResourceList& );
            void onGetCamerasDone( int reqID, const ErrorCode, const QnVirtualCameraResourceList& );
            void onGetCamerasHistoryDone( int reqID, const ErrorCode, const QnCameraHistoryList& );
            void onGetCameraBookmarkTagsDone(int reqID, const ErrorCode, const QnCameraBookmarkTags& );
            void onGetUsersDone( int reqID, const ErrorCode, const QnUserResourceList& );
            void onGetBusinessRulesDone( int reqID, const ErrorCode, const QnBusinessEventRuleList& );
            void onGetLicensesDone( int reqID, const ErrorCode, const QnLicenseList& );
            void onGetLayoutsDone( int reqID, const ErrorCode, const QnLayoutResourceList& );
            void onGetStoredFileDone( int reqID, const ErrorCode, const QByteArray& );
            void onListDirectoryDone( int reqID, const ErrorCode, const QStringList& );
            void onCurrentTimeDone( int reqID, const ErrorCode, const qint64& );
            void onDumpDatabaseDone( int reqID, const ErrorCode, const ec2::ApiDatabaseDumpData& );
            void onTestConnectionDone( int reqID, const ErrorCode, const QnConnectionInfo& );
            void onConnectDone( int reqID, const ErrorCode, const AbstractECConnectionPtr &);
            void onAddVideowallDone( int reqID, const ErrorCode, const QnVideoWallResourceList& );
            void onGetVideowallsDone( int reqID, const ErrorCode, const QnVideoWallResourceList& );
        };




        //////////////////////////////////////////////////////////
        ///////// Common handlers
        //////////////////////////////////////////////////////////
        DEFINE_ONE_ARG_HANDLER( Simple, ErrorCode )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractResourceManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( SetResourceStatus, ErrorCode, QUuid )
        //DEFINE_TWO_ARG_HANDLER( SetResourceDisabled, ErrorCode, QUuid )
        DEFINE_TWO_ARG_HANDLER( GetResourceTypes, ErrorCode, QnResourceTypeList )
        DEFINE_TWO_ARG_HANDLER( GetResources, ErrorCode, QnResourceList )
        DEFINE_TWO_ARG_HANDLER( GetResource, ErrorCode, QnResourcePtr )
        DEFINE_TWO_ARG_HANDLER( GetKvPairs, ErrorCode, QnKvPairListsById )
        DEFINE_TWO_ARG_HANDLER( SaveKvPairs, ErrorCode, QnKvPairListsById )

        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractMediaServerManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( SaveServer, ErrorCode, QnMediaServerResourcePtr)
        DEFINE_TWO_ARG_HANDLER( SaveResource, ErrorCode, QnResourcePtr)
        DEFINE_TWO_ARG_HANDLER( GetServers, ErrorCode, QnMediaServerResourceList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractCameraManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( AddCamera, ErrorCode, QnVirtualCameraResourceList )
        DEFINE_TWO_ARG_HANDLER( GetCameras, ErrorCode, QnVirtualCameraResourceList )
        DEFINE_TWO_ARG_HANDLER( GetCamerasHistory, ErrorCode, QnCameraHistoryList )
        DEFINE_TWO_ARG_HANDLER( GetCameraBookmarkTags, ErrorCode, QnCameraBookmarkTags )

        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractUserManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetUsers, ErrorCode, QnUserResourceList )
        DEFINE_TWO_ARG_HANDLER( AddUser, ErrorCode, QnUserResourceList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractBusinessEventManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetBusinessRules, ErrorCode, QnBusinessEventRuleList )
        DEFINE_TWO_ARG_HANDLER( SaveBusinessRule, ErrorCode, QnBusinessEventRulePtr)

        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractLicenseManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetLicenses, ErrorCode, QnLicenseList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractLayoutManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetLayouts, ErrorCode, QnLayoutResourceList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractVideowallManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetVideowalls, ErrorCode, QnVideoWallResourceList )
        DEFINE_TWO_ARG_HANDLER( AddVideowall, ErrorCode, QnVideoWallResourceList )

        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractStoredFileManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetStoredFile, ErrorCode, QByteArray )
        DEFINE_TWO_ARG_HANDLER( ListDirectory, ErrorCode, QStringList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractECConnection
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( CurrentTime, ErrorCode, qint64 )
        DEFINE_TWO_ARG_HANDLER( DumpDatabase, ErrorCode, ApiDatabaseDumpData )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractECConnectionFactory
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( TestConnection, ErrorCode, QnConnectionInfo )
        DEFINE_TWO_ARG_HANDLER( Connect, ErrorCode, AbstractECConnectionPtr )
    }

    Q_ENUMS( ErrorCode );
}

Q_DECLARE_METATYPE( ec2::ErrorCode );
Q_DECLARE_METATYPE( ec2::AbstractECConnectionPtr );

#endif  //EC_API_IMPL_H
