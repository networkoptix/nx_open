#include "event_processors.h"

#include <cassert>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>

#include <nx/utils/log/assert.h>


// -------------------------------------------------------------------------- //
// QnEventSignalEmitter
// -------------------------------------------------------------------------- //
void QnEventSignalEmitter::activate(int signalIndex, QObject *watched, QEvent *event) {
    NX_ASSERT(signalIndex >= 0 && signalIndex < SignalCount);

    switch(signalIndex) {
#define CASE(Z, N, D)                                                           \
    case N:                                                                     \
        emit BOOST_PP_CAT(activate, N)(watched, event);                         \
        return;
    BOOST_PP_REPEAT(QN_EVENT_SIGNAL_EMITTER_SIGNAL_COUNT, CASE, ~)
#undef CASE
    default:
        break;
    }
}

const char *QnEventSignalEmitter::signature(int signalIndex) {
    NX_ASSERT(signalIndex >= 0 && signalIndex < SignalCount);

    switch(signalIndex) {
#define CASE(Z, N, D)                                                           \
    case N:                                                                     \
        return BOOST_PP_STRINGIZE(QSIGNAL_CODE) BOOST_PP_STRINGIZE(BOOST_PP_CAT(activate, N)) BOOST_PP_STRINGIZE((QObject*,QEvent*));
    BOOST_PP_REPEAT(QN_EVENT_SIGNAL_EMITTER_SIGNAL_COUNT, CASE, ~)
    default:
        return NULL;
    }
}


// -------------------------------------------------------------------------- //
// QnEventSignalEmitterPool
// -------------------------------------------------------------------------- //
QnEventSignalEmitterPool::QnEventSignalEmitterPool(QObject *parent):
    base_type(parent)
{
    grow(this);
}

void QnEventSignalEmitterPool::grow(QnEventSignalEmitter *emitter) {
    m_freeList.reserve(m_freeList.size() + QnEventSignalEmitter::SignalCount);
    for(int i = 0; i < QnEventSignalEmitter::SignalCount; i++)
        m_freeList.push_back(Signal(emitter, i));
}

QnEventSignalEmitterPool::Signal QnEventSignalEmitterPool::allocate() {
    if(m_freeList.isEmpty())
        grow(new QnEventSignalEmitter(this));

    Signal result = m_freeList.back();
    m_freeList.pop_back();
    return result;
}

void QnEventSignalEmitterPool::release(const Signal &signal)
{
    NX_ASSERT(signal.sender == this || signal.sender->parent() == this);

    m_freeList.push_back(signal);
}
