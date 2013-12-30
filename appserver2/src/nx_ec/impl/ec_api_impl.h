
#ifndef EC_API_IMPL_H
#define EC_API_IMPL_H

#include <memory>

#include <api/model/kvpair.h>
#include <business/business_fwd.h>
#include <core/resource/camera_history.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_type.h>
#include <licensing/license.h>


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



namespace ec2
{
    class AbstractECConnection;

    enum class ErrorCode
    {
        ok,
        failure
    };

    namespace impl
    {
        template<class Param1, class Param2>
        class TwoParamHandler
        {
        public:
            typedef Param1 first_type;
            typedef Param2 second_type;

            virtual ~TwoParamHandler() {}
            virtual void done( const Param1& val1, const Param2& val2 ) = 0;
        };

        template<class Func, class BaseType>
        class CustomTwoParamHandler
        :
            public BaseType
        {
        public:
            CustomTwoParamHandler( const Func& func ) : m_func( func ) {}
            virtual void done( const typename BaseType::first_type& val1, const typename BaseType::second_type& val2 ) { m_func( val1, val2 ); };
        private:
            Func m_func;
        };


        //////////////////////////////////////////////////////////
        ///////// Common handlers
        //////////////////////////////////////////////////////////
        class SimpleHandler
        {
        public:
            virtual ~SimpleHandler() {}
            virtual void done( ErrorCode errorCode ) = 0;
        };
        typedef std::shared_ptr<SimpleHandler> SimpleHandlerPtr;

        template<class Func>
        class CustomSimpleHandler
        :
            public SimpleHandler
        {
        public:
            CustomSimpleHandler( Func func ) : m_func( func ) {}
            virtual void done( ErrorCode errorCode ) { m_func( errorCode ); }
        private:
            Func m_func;
        };


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
        class SaveServerHandler
        {
        public:
            virtual ~SaveServerHandler() {}
            virtual void done( ErrorCode errorCode, const QnMediaServerResourceList& servers, const QByteArray& authKey ) = 0;
        };
        typedef std::shared_ptr<SaveServerHandler> SaveServerHandlerPtr;

        template<class Func>
        class CustomSaveServerHandler
        :
            public SaveServerHandler
        {
        public:
            CustomSaveServerHandler( Func func ) : m_func( func ) {}
            virtual void done( ErrorCode errorCode, const QnMediaServerResourceList& servers, const QByteArray& authKey ) { m_func( errorCode, servers, authKey ); }
        private:
            Func m_func;
        };

        DEFINE_TWO_ARG_HANDLER( GetServers, ErrorCode, QnMediaServerResourceList )


        //////////////////////////////////////////////////////////
        ///////// Handlers for AbstractCameraManager
        //////////////////////////////////////////////////////////
        DEFINE_TWO_ARG_HANDLER( AddCamera, ErrorCode, QnVirtualCameraResourceList )
        DEFINE_TWO_ARG_HANDLER( GetCameras, ErrorCode, QnVirtualCameraResourceList )
        DEFINE_TWO_ARG_HANDLER( GetCamerasHistory, ErrorCode, QnCameraHistoryList )
        

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
        DEFINE_TWO_ARG_HANDLER( Connect, ErrorCode, AbstractECConnection )
    }
}

#endif  //EC_API_IMPL_H
