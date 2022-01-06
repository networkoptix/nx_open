// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "kinetic_processor.h"

#include <ui/animation/animation_timer.h>

#include <nx/utils/log/log.h>

namespace {

static constexpr qreal kDefaultMaxShiftIntervalMs = 200;
static constexpr int kDefaultMaxShiftCount = 10;

} // namespace

KineticProcessor::KineticProcessor(QMetaType type, QObject *parent):
    QObject(parent),
    mState(Measuring),
    mType(type),
    mHandler(nullptr),
    mMaxShiftCount(kDefaultMaxShiftCount),
    mMaxShiftInterval(kDefaultMaxShiftIntervalMs / 1000.0),
    mMinSpeedMagnitude(0.0),
    mMaxSpeedMagnitude(std::numeric_limits<qreal>::max()),
    mFriction(1.0)
{
    mMagnitudeCalculator = MagnitudeCalculator::forType(type);
    if(mMagnitudeCalculator == nullptr) {
        NX_ASSERT(false, "No magnitude calculator is registered for the given type '%1'.", type.name());
        mMagnitudeCalculator = MagnitudeCalculator::forType({});
    }

    mLinearCombinator = LinearCombinator::forType(type);
    if(mLinearCombinator == nullptr) {
        NX_ASSERT(false, "No linear combinator is registered for the given type '%1'.", type.name());
        mLinearCombinator = LinearCombinator::forType({});
    }

    reset();
}

KineticProcessor::~KineticProcessor() {
    setHandler(nullptr);
}

void KineticProcessor::reset() {
    transition(Measuring);

    mShifts.clear();
    mLastShiftTimeMSec = QDateTime::currentMSecsSinceEpoch();
}

void KineticProcessor::shift(const QVariant &dv) {
    if(dv.metaType() != mType)
    {
        NX_ASSERT(
            false,
            "Spatial displacement of invalid type was given - expected type '%1', got type '%2'.",
            mType.name(),
            dv.metaType().name());
        return;
    }

    /* QDateTime::currentMSecsSinceEpoch gives the most accurate timestamp on
     * Windows, even more accurate than QElapsedTimer. Not monotonic, so we
     * may want to create a monotonic wrapper one day. */
    qint64 currentTimeMSec = QDateTime::currentMSecsSinceEpoch();

    Shift shift;
    shift.dv = dv;
    shift.dt = (currentTimeMSec - mLastShiftTimeMSec) / 1000.0;
    if(shift.dt > mMaxShiftInterval) {
        /* New motion has started. */
        mShifts.clear();
        shift.dt = mMaxShiftInterval;
    } else if(mFlags & IgnoreDeltaTime) {
        shift.dt = mMaxShiftInterval;
    }
    mShifts.append(shift);

    mLastShiftTimeMSec = currentTimeMSec;

    if (mShifts.size() > mMaxShiftCount)
        mShifts.takeFirst();
}

void KineticProcessor::setHandler(KineticProcessHandler *handler) {
    if(handler != nullptr && handler->mProcessor != nullptr) {
        NX_ASSERT(false, "Given handler is already assigned to a processor.");
        return;
    }

    if(mHandler != nullptr) {
        reset();
        mHandler->mProcessor = nullptr;
    }

    mHandler = handler;

    if(mHandler != nullptr)
        mHandler->mProcessor = this;
}

void KineticProcessor::setMaxShiftCount(int count) {
    if(count < 1) {
        NX_ASSERT(false, "Invalid maximal shift count '%1'.", count);
        return;
    }

    mMaxShiftCount = count;
}

void KineticProcessor::setMinSpeedMagnitude(qreal minSpeedMagnitude) {
    if(minSpeedMagnitude < 0.0) {
        NX_ASSERT(false, "Invalid minimal speed magnitude %1", minSpeedMagnitude);
        return;
    }

    mMinSpeedMagnitude = minSpeedMagnitude;
}

void KineticProcessor::setFriction(qreal friction) {
    if(friction < 0.0) {
        NX_ASSERT(false, "Invalid friction value '%1'", friction);
        return;
    }

    mFriction = friction;
}

QVariant KineticProcessor::calculateSpeed(const ShiftList &shifts) const {
    if (shifts.size() == 0)
        return mLinearCombinator->zero();

    QVariant sumValue = mLinearCombinator->zero();
    qreal sumTime = 0;
    foreach(const Shift &shift, shifts) {
        sumValue = mLinearCombinator->combine(1.0, sumValue, 1.0, shift.dv);
        sumTime += shift.dt;
    }

    if (sumTime == 0)
        sumTime = mMaxShiftInterval; /* Technically speaking, we should never get here, but we want to feel safe. */

    return mLinearCombinator->combine(1.0 / sumTime, sumValue);
}

QVariant KineticProcessor::calculateSpeed() {
    /* Check if the motion has expired. */
    if(!mShifts.empty() && (QDateTime::currentMSecsSinceEpoch() - mLastShiftTimeMSec) / 1000.0 > mMaxShiftInterval)
        mShifts.clear();

    QVariant result = calculateSpeed(mShifts);
    mShifts.clear();

    return result;
}

QVariant KineticProcessor::updateSpeed(const QVariant &initialSpeed, const QVariant &currentSpeed, const QVariant &speedGain, qreal dt) const {
    Q_UNUSED(initialSpeed);

    QVariant speed = mLinearCombinator->combine(1.0, currentSpeed, 1.0, speedGain);

    qreal oldMagnitude = mMagnitudeCalculator->calculate(speed);
    qreal newMagnitude = oldMagnitude - mFriction * dt;
    if(newMagnitude <= 0.0)
        return mLinearCombinator->zero();

    return mLinearCombinator->combine(newMagnitude / oldMagnitude, speed);
}

void KineticProcessor::tick(int deltaTimeMSec) {
    /* Update current speed. */
    QVariant speedGain = calculateSpeed();
    qreal dt = deltaTimeMSec / 1000.0;
    mCurrentSpeed = updateSpeed(mInitialSpeed, mCurrentSpeed, speedGain, dt);

    /* Adjust for max magnitude. */
    qreal currentSpeedMagnitude = mMagnitudeCalculator->calculate(mCurrentSpeed);
    if(currentSpeedMagnitude > mMaxSpeedMagnitude)
        mCurrentSpeed = mLinearCombinator->combine(mMaxSpeedMagnitude / currentSpeedMagnitude, mCurrentSpeed);

    if(currentSpeedMagnitude <= mMinSpeedMagnitude) {
        reset();
    } else {
        if(!qFuzzyIsNull(dt) && mHandler != nullptr)
            mHandler->kineticMove(mLinearCombinator->combine(dt, mCurrentSpeed));
    }
}

void KineticProcessor::transition(State state) {
    if(mState == state)
        return;

    mState = state;

    switch(state) {
    case Measuring: /* Running -> Measuring. */
        stopListening();
        if(mHandler != nullptr)
            mHandler->finishKinetic();
        return;
    case Running: /* Measuring -> Running. */
        mInitialSpeed = mCurrentSpeed = calculateSpeed();
        if(mState == state) { /* State may get changed in a callback. */
            if(timer() == nullptr) {
                AnimationTimer *timer = new QAnimationTimer(this);
                timer->addListener(this);
            }

            startListening();
            if(mState == state && mHandler != nullptr) /* And again, state may have changed. */
                mHandler->startKinetic();
        }
        return;
    default:
        return;
    }
}


KineticProcessHandler::~KineticProcessHandler() {
    if(mProcessor != nullptr)
        mProcessor->setHandler(nullptr);
}
