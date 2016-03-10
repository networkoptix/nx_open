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
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_type.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <licensing/license.h>


//TODO #ak Defining multiple handler macro because vs2012 does not support variadic templates. Will move to single variadic template after move to vs2013

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
            QObject::connect( static_cast<AppServerSignaller*>(this), &AppServerSignaller::on##REQUEST_NAME##Done, target, func, connectionType); \
        }                                                                               \
        virtual void done(int reqID, const FIRST_ARG_TYPE& val1 ) override { on##REQUEST_NAME##Done( reqID, val1); }; \
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
            QObject::connect( static_cast<AppServerSignaller*>(this), &AppServerSignaller::on##REQUEST_NAME##Done, target, func, connectionType); \
        } \
        virtual void done( \
            int reqID, \
            const FIRST_ARG_TYPE& val1, \
            const SECOND_ARG_TYPE& val2 ) override { on##REQUEST_NAME##Done( reqID, val1, val2); }; \
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
            QObject::connect( static_cast<AppServerSignaller*>(this), &AppServerSignaller::on##REQUEST_NAME##Done, target, func, connectionType); \
        } \
        virtual void done( \
            int reqID, \
            const FIRST_ARG_TYPE& val1, \
            const SECOND_ARG_TYPE& val2, \
            const THIRD_ARG_TYPE& val3 ) override { on##REQUEST_NAME##Done( reqID, val1, val2, val3); }; \
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
        //!Can't check authorization because of LDAP server is offline
        temporary_unauthorized,
        //!Requested operation is currently forbidden (e.g., read-only mode is enabled)
        forbidden,
        //!Response parse error
        badResponse,
        //!Error executing DB request
        dbError,
        containsBecauseTimestamp, // transaction already in database
        containsBecauseSequence,  // transaction already in database
        //!Method is not implemented yet
        notImplemented,
        //!Connection to peer is impossible due to incompatible protocol
        incompatiblePeer
    };

    QString toString( ErrorCode errorCode);

    namespace impl
    {
        template<class Param1>
        class OneParamHandler
        {
        public:
            typedef Param1 first_type;

            virtual ~OneParamHandler() {}
            virtual void done(int reqID, const Param1& val1 ) = 0;
        };


        template<class Param1, class Param2>
        class TwoParamHandler
        {
        public:
            typedef Param1 first_type;
            typedef Param2 second_type;

            virtual ~TwoParamHandler() {}
            virtual void done(int reqID, const Param1& val1, const Param2& val2 ) = 0;
        };


        template<class Param1, class Param2, class Param3>
        class ThreeParamHandler
        {
        public:
            typedef Param1 first_type;
            typedef Param2 second_type;
            typedef Param3 third_type;

            virtual ~ThreeParamHandler() {}
            virtual void done(int reqID, const Param1& val1, const Param2& val2, const Param3& val3 ) = 0;
        };


        class AppServerSignaller: public QObject
        {
            Q_OBJECT

        signals:
            void onSimpleDone                   (int reqID, const ec2::ErrorCode);
            void onGetResourceTypesDone         (int reqID, const ec2::ErrorCode, const QnResourceTypeList&);
            void onSetResourceStatusDone        (int reqID, const ec2::ErrorCode, const QnUuid&);
            void onSaveResourceDone             (int reqID, const ec2::ErrorCode, const QnResourcePtr&);
            void onGetResourcesDone             (int reqID, const ec2::ErrorCode, const QnResourceList&);
            void onGetResourceDone              (int reqID, const ec2::ErrorCode, const QnResourcePtr&);
            void onGetKvPairsDone               (int reqID, const ec2::ErrorCode, const ApiResourceParamWithRefDataList&);
            void onGetStatusListDone            (int reqID, const ec2::ErrorCode, const ApiResourceStatusDataList&);
            void onSaveKvPairsDone              (int reqID, const ec2::ErrorCode, const ApiResourceParamWithRefDataList&);
            void onSaveServerDone               (int reqID, const ec2::ErrorCode, const QnMediaServerResourcePtr&);
            void onSaveBusinessRuleDone         (int reqID, const ec2::ErrorCode, const QnBusinessEventRulePtr&);
            void onGetServersDone               (int reqID, const ec2::ErrorCode, const QnMediaServerResourceList&);
            void onGetServerUserAttributesDone  (int reqID, const ec2::ErrorCode, const QnMediaServerUserAttributesList&);
            void onGetStoragesDone              (int reqID, const ec2::ErrorCode, const QnResourceList&);
            void onGetCamerasDone               (int reqID, const ec2::ErrorCode, const ec2::ApiCameraDataList&);
            void onGetCameraUserAttributesDone  (int reqID, const ec2::ErrorCode, const QnCameraUserAttributesList&);
            void onGetCamerasHistoryDone        (int reqID, const ec2::ErrorCode, const ApiServerFootageDataList&);
            void onGetUsersDone                 (int reqID, const ec2::ErrorCode, const ec2::ApiUserDataList&);
            void onGetBusinessRulesDone         (int reqID, const ec2::ErrorCode, const QnBusinessEventRuleList&);
            void onGetLicensesDone              (int reqID, const ec2::ErrorCode, const QnLicenseList&);
            void onGetLayoutsDone               (int reqID, const ec2::ErrorCode, const ec2::ApiLayoutDataList&);
            void onGetStoredFileDone            (int reqID, const ec2::ErrorCode, const QByteArray&);
            void onListDirectoryDone            (int reqID, const ec2::ErrorCode, const QStringList&);
            void onCurrentTimeDone              (int reqID, const ec2::ErrorCode, const qint64&);
            void onDumpDatabaseDone             (int reqID, const ec2::ErrorCode, const ec2::ApiDatabaseDumpData&);
            void onGetDiscoveryDataDone         (int reqID, const ec2::ErrorCode, const ApiDiscoveryDataList&);
            void onTestConnectionDone           (int reqID, const ec2::ErrorCode, const QnConnectionInfo&);
            void onConnectDone                  (int reqID, const ec2::ErrorCode, const AbstractECConnectionPtr &);
            void onGetVideowallsDone            (int reqID, const ec2::ErrorCode, const ec2::ApiVideowallDataList&);
            void onGetWebPagesDone              (int reqID, const ec2::ErrorCode, const ec2::ApiWebPageDataList&);
        };




        //////////////////////////////////////////////////////////
        ///////// Common handlers
        //////////////////////////////////////////////////////////
        DEFINE_ONE_ARG_HANDLER( Simple, ErrorCode )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractResourceManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( SetResourceStatus, ErrorCode, QnUuid )
        //DEFINE_TWO_ARG_HANDLER( SetResourceDisabled, ErrorCode, QnUuid )
        DEFINE_TWO_ARG_HANDLER( GetResourceTypes, ErrorCode, QnResourceTypeList )
        DEFINE_TWO_ARG_HANDLER( GetResources, ErrorCode, QnResourceList )
        DEFINE_TWO_ARG_HANDLER( GetResource, ErrorCode, QnResourcePtr )
        DEFINE_TWO_ARG_HANDLER( GetKvPairs, ErrorCode, ApiResourceParamWithRefDataList )
        DEFINE_TWO_ARG_HANDLER( GetStatusList, ErrorCode, ApiResourceStatusDataList )
        DEFINE_TWO_ARG_HANDLER( SaveKvPairs, ErrorCode, ApiResourceParamWithRefDataList )

        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractMediaServerManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( SaveServer, ErrorCode, QnMediaServerResourcePtr)
        DEFINE_TWO_ARG_HANDLER( SaveResource, ErrorCode, QnResourcePtr)
        DEFINE_TWO_ARG_HANDLER( GetServers, ErrorCode, QnMediaServerResourceList )
        DEFINE_TWO_ARG_HANDLER( GetServerUserAttributes, ErrorCode, ApiMediaServerUserAttributesDataList)
        DEFINE_TWO_ARG_HANDLER( GetStorages, ErrorCode, QnResourceList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractCameraManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetCameras, ErrorCode, ec2::ApiCameraDataList )
        DEFINE_TWO_ARG_HANDLER( GetCameraUserAttributes, ErrorCode, ec2::ApiCameraAttributesDataList )
        DEFINE_TWO_ARG_HANDLER( GetCamerasHistory, ErrorCode, ec2::ApiServerFootageDataList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractUserManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetUsers, ErrorCode, ec2::ApiUserDataList )


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
        DEFINE_TWO_ARG_HANDLER( GetLayouts, ErrorCode, ec2::ApiLayoutDataList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractVideowallManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetVideowalls, ErrorCode, ec2::ApiVideowallDataList)

        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractWebPageManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetWebPages, ErrorCode, ec2::ApiWebPageDataList)

        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractStoredFileManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetStoredFile, ErrorCode, QByteArray )
        DEFINE_TWO_ARG_HANDLER( ListDirectory, ErrorCode, QStringList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractDiscoveryManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( GetDiscoveryData, ErrorCode, ApiDiscoveryDataList )


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

    Q_ENUMS( ErrorCode);
}

Q_DECLARE_METATYPE( ec2::ErrorCode);
Q_DECLARE_METATYPE( ec2::AbstractECConnectionPtr);

#endif  //EC_API_IMPL_H
