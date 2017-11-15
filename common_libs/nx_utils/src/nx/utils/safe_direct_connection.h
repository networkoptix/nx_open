/**********************************************************
* Jul 17, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_SAFE_DIRECT_CONNECTION_H
#define NX_SAFE_DIRECT_CONNECTION_H

#include <functional>
#include <list>
#include <memory>
#include <type_traits>

#include <QtCore/QObject>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "functor_proxy_helper.h"
#include <nx/utils/singleton.h>


namespace Qn
{
    class SafeDirectConnectionGlobalHelper;

    //!Class that want to receive signals directly, MUST inherit this type
    /*!
        \warning Implementation MUST call EnableSafeDirectConnection::directDisconnectAll before object destruction
    */
    class NX_UTILS_API EnableSafeDirectConnection
    {
    public:
#ifdef __arm__
        //#ak ISD Jaguar lacks kernel support for atomic int64
        typedef unsigned int ID;
#else
        typedef quint64 ID;
#endif

        EnableSafeDirectConnection();
        virtual ~EnableSafeDirectConnection();

        //!Disconnects from all directly-connected slot connected with directConnect
        void directDisconnectAll();
        //!Using integer sequence as a unique object id since pointer can be reused immediately after its free
        ID uniqueObjectSequence() const;

    private:
        std::shared_ptr<SafeDirectConnectionGlobalHelper> m_globalHelper;
        const ID m_uniqueObjectSequence;
    };

    /*!
        All signals, connected with Qn::safe_direct_connect, are delivered 
        through global instance of this class
    */
    class NX_UTILS_API SafeDirectConnectionGlobalHelper
    :
        public QObject/*,
        public Singleton<SafeDirectConnectionGlobalHelper>*/
    {
        Q_OBJECT

    public:
        /*!
            Overload for slot as a member function
        */
        template<
            class SenderType, class SignalType,
            class ReceiverType, class SlotType            
        >
            void directConnect(
                const SenderType* sender, SignalType signalPtr,
                ReceiverType* receiver, SlotType slotPtr,
                typename std::enable_if<std::is_member_function_pointer<SlotType>::value>::type* = nullptr )
        {
            directConnectInternal( sender, signalPtr, receiver, nx::bind_only_1st( slotPtr, receiver ) );
        }

        /*!
            Overload for slot as a functor
        */
        template<
            class SenderType, class SignalType,
            class ReceiverType, class SlotType            
        >
            void directConnect(
                const SenderType* sender, SignalType signalPtr,
                ReceiverType* receiver, SlotType slotFunc,
                typename std::enable_if<!std::is_member_function_pointer<SlotType>::value>::type* = nullptr )
        {
            directConnectInternal( sender, signalPtr, receiver, slotFunc );
        }

        //!Safely disconnects receiver from all signals
        /*!
            NOTE: Blocks till currently executed slots return
        */
        void directDisconnectAll( const EnableSafeDirectConnection* receiver );
        //!Returns true if receiver is still connected to some signal
        bool isConnected( const EnableSafeDirectConnection* receiver ) const;

        /*!
            NOTE: By using std::shared_ptr we ensure that SafeDirectConnectionGlobalHelper instance
                is destroyed not earlier then last EnableSafeDirectConnection instance
        */
        static std::shared_ptr<SafeDirectConnectionGlobalHelper> instance();

    private:
        struct ReceiverContext
        {
            int slotsInvokedCounter;
            std::list<QMetaObject::Connection> connections;
            bool terminated;

            ReceiverContext()
            :
                slotsInvokedCounter(0),
                terminated(false)
            {
            }
        };

        mutable QnMutex m_mutex;
        QnWaitCondition m_cond;
        //!map<receiver, counter>
        std::map<EnableSafeDirectConnection::ID, ReceiverContext> m_receivers;

        template<
            class SenderType, class SignalType,
            class ReceiverType, class SlotType            
        >
            void directConnectInternal(
                const SenderType* sender, SignalType signalPtr,
                ReceiverType* receiver, SlotType slotFunc )
        {
            auto proxyFunc = nx::createProxyFunc(
                slotFunc,
                std::bind(
                    &SafeDirectConnectionGlobalHelper::beforeSlotInvoked, this,
                    sender, receiver->uniqueObjectSequence() ),
                std::bind(
                    &SafeDirectConnectionGlobalHelper::afterSlotInvoked, this,
                    sender, receiver->uniqueObjectSequence() ),
                signalPtr );

            auto connection = connect( sender, signalPtr, this, proxyFunc, Qt::DirectConnection );
            newSafeConnectionEstablished( receiver->uniqueObjectSequence(), std::move(connection) );
        }

        void newSafeConnectionEstablished(
            EnableSafeDirectConnection::ID receiverID,
            QMetaObject::Connection connection );
        bool beforeSlotInvoked( const QObject* sender, EnableSafeDirectConnection::ID receiver );
        void afterSlotInvoked( const QObject* sender, EnableSafeDirectConnection::ID receiver );
    };

    //!MUST be used instead of QObject::connect with Qt::DirectConnection specified to guarantee thread-safety while disconnecting
    /*!
        NOTE: overload for slot as a member function
    */
    template<
        typename SenderType, typename SignalType,
        typename ReceiverType, typename SlotType
    >
        void directConnect(
            const SenderType* sender, SignalType signalFunc,
            ReceiverType* receiver, SlotType slotFunc )
    {
        return SafeDirectConnectionGlobalHelper::instance()->directConnect(
            sender, signalFunc, receiver, slotFunc );
    }

    //!Safely disconnects receiver from all signals
    /*!
        NOTE: Blocks till currently executed slots return
    */
    void NX_UTILS_API directDisconnectAll( const EnableSafeDirectConnection* receiver );

    //!Waits for currently running slots to exit
    /*!
        \param sender Can be NULL
        \param receiver Can be NULL
    */
    //template<class SignalType, class SlotType>
    //    void directDisconnect(
    //        const QObject* sender, SignalType signalFunc,
    //        EnableSafeDirectConnection* receiver, SlotType slotFunc )
    //{
    //    SafeDirectConnectionGlobalHelper::instance()->directDisconnect(
    //        sender, signalFunc, receiver, slotFunc );
    //}
}

#endif  //NX_SAFE_DIRECT_CONNECTION_H
