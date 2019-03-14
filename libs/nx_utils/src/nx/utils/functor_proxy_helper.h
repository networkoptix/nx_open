#pragma once

#include <functional>

namespace nx {

    //TODO #ak refactor following code using variadic template when available

/*!
    \param NAME 2
    \param TYPENAME_ARG_LIST    , typename Arg1, typename Arg2
    \param ARG_PARAM_LIST       Arg1 arg1, Arg2 arg2
    \param ARG_VAR_LIST         arg1, arg2
    \param ARG_TYPE_LIST        Arg1, Arg2
    \param ARG_TYPE_LIST_COMMA  , Arg1, Arg2
    \param PLACEHOLDERS_WITH_COMMA , _1, _2
*/
#define DEFINE_PROXY_FUNC_HELPERS( NAME, TYPENAME_ARG_LIST, ARG_PARAM_LIST, ARG_VAR_LIST, ARG_TYPE_LIST, ARG_TYPE_LIST_COMMA, PLACEHOLDERS_WITH_COMMA ) \
    template<typename R TYPENAME_ARG_LIST>      \
    class ProxyFunc##NAME                       \
    {                                           \
    public:                                     \
        template<class TargetFuncType>          \
        ProxyFunc##NAME(                        \
            TargetFuncType target,              \
            std::function<bool()> funcToCallBefore, \
            std::function<void()> funcToCallAfter ) \
        :                                           \
            m_targetFunc( target ),                 \
            m_funcToCallBefore( funcToCallBefore ), \
            m_funcToCallAfter( funcToCallAfter )    \
        {                                           \
        }                                           \
                                                    \
        R operator()( ARG_PARAM_LIST )              \
        {                                           \
            if( !m_funcToCallBefore || m_funcToCallBefore() )   \
                m_targetFunc( ARG_VAR_LIST );                   \
            if( m_funcToCallAfter )                             \
                m_funcToCallAfter();                            \
        }                                                       \
                                                                \
    private:                                                    \
        std::function<R( ARG_TYPE_LIST )> m_targetFunc;         \
        std::function<bool()> m_funcToCallBefore;               \
        std::function<void()> m_funcToCallAfter;                \
    };                                                          \
                                                                \
    template<typename TargetFuncType, typename R, typename SenderType TYPENAME_ARG_LIST>    \
    ProxyFunc##NAME<R ARG_TYPE_LIST_COMMA> createProxyFunc(                                 \
        TargetFuncType target,                                                              \
        std::function<bool()> funcToCallBefore,                                             \
        std::function<void()> funcToCallAfter,                                              \
        R( SenderType::* /*dummySignalPtr*/ )(ARG_TYPE_LIST) )                              \
    {                                                                                       \
        return ProxyFunc##NAME<R ARG_TYPE_LIST_COMMA>( target, funcToCallBefore, funcToCallAfter ); \
    }                                                                                               \
                                                                                                    \
    template<typename T, typename R, class ObjType TYPENAME_ARG_LIST>                               \
    std::function<R( ARG_TYPE_LIST )> bind_only_1st( R( T::*func )(ARG_TYPE_LIST), ObjType* obj )   \
    {                                                                                               \
        return std::bind( func, obj PLACEHOLDERS_WITH_COMMA );                                      \
    }                                                                                               \
                                                                                                    \


#define COMMA ,


    DEFINE_PROXY_FUNC_HELPERS(
        0,
        ,
        ,
        ,
        ,
        ,
         )
    DEFINE_PROXY_FUNC_HELPERS(
        1,
        COMMA typename Arg1,
        Arg1 arg1,
        arg1,
        Arg1,
        COMMA Arg1,
        COMMA std::placeholders::_1 )
    DEFINE_PROXY_FUNC_HELPERS(
        2,
        COMMA typename Arg1 COMMA typename Arg2,
        Arg1 arg1 COMMA Arg2 arg2,
        arg1 COMMA arg2,
        Arg1 COMMA Arg2,
        COMMA Arg1 COMMA Arg2,
        COMMA std::placeholders::_1 COMMA std::placeholders::_2 )
    DEFINE_PROXY_FUNC_HELPERS(
        3,
        COMMA typename Arg1 COMMA typename Arg2 COMMA typename Arg3,
        Arg1 arg1 COMMA Arg2 arg2 COMMA Arg3 arg3,
        arg1 COMMA arg2 COMMA arg3,
        Arg1 COMMA Arg2 COMMA Arg3,
        COMMA Arg1 COMMA Arg2 COMMA Arg3,
        COMMA std::placeholders::_1 COMMA std::placeholders::_2 COMMA std::placeholders::_3 )
    DEFINE_PROXY_FUNC_HELPERS(
        4,
        COMMA typename Arg1 COMMA typename Arg2 COMMA typename Arg3 COMMA typename Arg4,
        Arg1 arg1 COMMA Arg2 arg2 COMMA Arg3 arg3 COMMA Arg4 arg4,
        arg1 COMMA arg2 COMMA arg3 COMMA arg4,
        Arg1 COMMA Arg2 COMMA Arg3 COMMA Arg4,
        COMMA Arg1 COMMA Arg2 COMMA Arg3 COMMA Arg4,
        COMMA std::placeholders::_1 COMMA std::placeholders::_2 COMMA std::placeholders::_3 COMMA std::placeholders::_4 )

} // namespace nx
