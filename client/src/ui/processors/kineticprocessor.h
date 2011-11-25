#ifndef QN_KINETIC_PROCESSOR_H
#define QN_KINETIC_PROCESSOR_H

#include <limits>
#include <QList>
#include <QDateTime>
#include <ui/animation/animation_timer.h>

class QPointF;

namespace detail {
    qreal calculateMagnitude(float value);
    qreal calculateMagnitude(double value);
    qreal calculateMagnitude(const QPointF &point);

    template<class T>
    qreal calculateMagnitude(const T &value, ...) {
        qnWarning("calculateMagnitude function is not implemented for type '%1'.", typeid(T).name());
        return 0.0;
    }

} // namespace detail


template<class T>
class KineticProcessor;

/**
 * This interface is to be implemented by the receiver of kinetic events.
 * 
 * It is to be used in conjunction with <tt>KineticProcessor</tt>.
 */
template<class T>
class KineticProcessHandler {
public:
    /**
     * Default constructor.
     */
    KineticProcessHandler(): mProcessor(NULL) {}
    
    /**
     * Virtual destructor.
     */
    virtual ~KineticProcessHandler();

    /**
     * This function is called whenever kinetic motion starts.
     * 
     * It is guaranteed that <tt>finishKinetic()</tt> will also be called.
     */
    virtual void startKinetic() {}

    /**
     * This function is called at regular time intervals throughout the kinetic
     * motion process.
     * 
     * \param distance                  Distance that was covered by kinetic 
     *                                  motion since the last call to this function.
     */
    virtual void moveKinetic(const T &distance) { unused(distance); };

    /**
     * This function is called whenever kinetic motion stops.
     * 
     * If <tt>startKinetic()</tt> was called, then it is guaranteed that
     * this function will also be called.
     */
    virtual void finishKinetic() {};

    /**
     * \returns                         Kinetic processor associated with this handler. 
     */
    KineticProcessor<T> *kineticProcessor() const {
        return mProcessor;
    }

private:
    friend class KineticProcessor<T>;

    KineticProcessor<T> *mProcessor;
};


/**
 * Kinetic processor implements logic that is common to components that make
 * use of kinetic motion.
 * 
 * Kinetic processor presents several functions that are to be used from user
 * code. The most important functions are:
 * <ul>
 * <li><tt>reset()</tt>, which is to be used when a motion starts, or when
 *    there is a need to stop kinetic process prematurely. </li>
 * <li><tt>shift()</tt>, which is be to used to feed position delta
 *    values to kinetic processor while motion is in progress.</tt>
 * <li><tt>start()</tt>, which starts kinetic motion.</tt>
 * </ul>
 * 
 * User code is notified about state changes and kinetic motion through the 
 * <tt>KineticProcessHandler</tt> interface. It is guaranteed that no matter 
 * what happens, functions of this interface will be called in proper order. 
 * For example, if <tt>startKinetic()</tt> function was called, 
 * then <tt>finishKinetic()</tt> will also be called even if this kinetic 
 * processor is destroyed.
 */
template<class T>
class KineticProcessor : public QObject, protected AnimationTimerListener {
public:
    /**
     * Kinetic state.
     */
    enum State {
        MEASURING, /**< Recording shifts. */
        KINETIC    /**< Performing kinetic motion. */
    };

    enum {
        DEFAULT_MAX_SHIFT_COUNT = 10,
        DEFAULT_MAX_SHIFT_INTERVAL_MSEC = 200
    };

    /**
     * Constructor.
     * 
     * \param parent                    Parent for this object.
     */
    KineticProcessor(QObject *parent = NULL):
        QObject(parent),
        mState(MEASURING),
        mTimer(new AnimationTimer(this)),
        mHandler(NULL),
        mMaxShiftCount(DEFAULT_MAX_SHIFT_COUNT),
        mMaxShiftInterval(DEFAULT_MAX_SHIFT_INTERVAL_MSEC / 1000.0),
        mMinSpeedMagnitude(0.0),
        mMaxSpeedMagnitude(std::numeric_limits<qreal>::max()),
        mFriction(1.0)
    {
        mTimer->setListener(this);
      
        reset();
    }

    /**
     * Virtual destructor.
     */
    virtual ~KineticProcessor() {
        setHandler(NULL);
    }

    /**
     * Resets this kinetic processor into its initial state.
     */
    void reset() {
        transition(MEASURING);

        mShifts.clear();
        mLastShiftTimeMSec = QDateTime::currentMSecsSinceEpoch();
    }

    /**
     * \param shift                     Spatial displacement since the last call
     *                                  to this function.
     */
    void shift(const T &dv) {
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
        }
        mShifts.append(shift);

        mLastShiftTimeMSec = currentTimeMSec;

        if (mShifts.size() > mMaxShiftCount)
            mShifts.takeFirst();
    }

    /**
     * Starts kinetic process.
     */
    void start() {
        transition(KINETIC);
    }

    /**
     * \returns                         Current state of this kinetic processor.
     */
    State state() const {
        return mState;
    }

    /**
     * \returns                         Handler for this kinetic processor.
     */
    KineticProcessHandler<T> *handler() const {
        return mHandler;
    }

    /**
     * \param handler                   New handler for this kinetic processor.
     */
    void setHandler(KineticProcessHandler<T> *handler) {
        if(handler != NULL && handler->mProcessor != NULL) {
            qnWarning("Given handler is already assigned to a processor.");
            return;
        }

        if(mHandler != NULL) {
            reset();
            mHandler->mProcessor = NULL;
        }

        mHandler = handler;

        if(mHandler != NULL)
            mHandler->mProcessor = this;
    }
    
    /**
     * \param count                     Maximal number of latest shifts that will
     *                                  be considered when computing initial
     *                                  kinetic speed. Good values are in range [5, 15].
     */
    void setMaxShiftCount(int count) {
        if(count < 1) {
            qnWarning("Invalid maximal shift count '%1'.", count);
            return;
        }

        mMaxShiftCount = count;
    }

    int maxShiftCount() const {
        return mMaxShiftCount;
    }

    /**
     * \param minSpeedMagnitude         Minimal speed magnitude. When current speed 
     *                                  magnitude becomes less or equal to this value,
     *                                  kinetic motion stops.
     */
    void setMinSpeedMagnitude(qreal minSpeedMagnitude) {
        if(minSpeedMagnitude < 0.0) {
            qnWarning("Invalid minimal speed magnitude %1", minSpeedMagnitude);
            return;
        }

        mMinSpeedMagnitude = minSpeedMagnitude;
    }

    qreal minSpeedMagnitude() const {
        return mMinSpeedMagnitude;
    }

    /**
     * \param maxSpeedMagnitude         Maximal speed magnitude. Current speed
     *                                  magnitude cannot become greater than this value.
     */
    void setMaxSpeedMagnitude(qreal maxSpeedMagnitude) {
        mMaxSpeedMagnitude = maxSpeedMagnitude;
    }

    qreal maxSpeedMagnitude() const {
        return mMaxSpeedMagnitude;
    }

    /**
     * \param interval                  Maximal interval between consequent calls
     *                                  to <tt>shift()</tt> for them to be considered
     *                                  a part of a single motion, in seconds.
     */
    void setMaxShiftInterval(qreal interval) {
        mMaxShiftInterval = interval;
    }

    qreal maxShiftInterval() const {
        return mMaxShiftInterval;
    }

    /**
     * \param friction                  Friction coefficient. Set to +inf to disable kinetic motion, set to 0.0 to make it never stop.
     */
    void setFriction(qreal friction) {
        if(friction < 0.0) {
            qnWarning("Invalid friction value '%1'", friction);
            return;
        }

        mFriction = friction;
    }

    qreal friction() const {
        return mFriction;
    }

protected:
    struct Shift {
        T dv;       /**< Spatial displacement. */
        qreal dt;   /**< Time delta, in seconds. */
    };

    typedef QList<Shift> ShiftList;

    /**
     * Given a valid list of shifts, this function calculates speed.
     * 
     * Default implementation just takes the mean. If some other behavior is 
     * desired, this function can be reimplemented in derived class.
     * 
     * \param shifts                    List of shifts.
     * \returns                         Estimated speed, in T per second.
     */
    virtual T calculateSpeed(const ShiftList &shifts) const {
        if (shifts.size() == 0)
            return T();

        T sumValue = T();
        qreal sumTime = 0;
        foreach(const Shift &shift, shifts) {
            sumValue += shift.dv;
            sumTime += shift.dt;
        }

        if (sumTime == 0)
            sumTime = mMaxShiftInterval; /* Technically speaking, we should never get here, but we want to feel safe. */

        return sumValue / sumTime;
    }

    /**
     * Calculates speed based on currently recorded shifts and clears them.
     */
    T calculateSpeed() {
        /* Check if the motion has expired. */
        if(!mShifts.empty() && (QDateTime::currentMSecsSinceEpoch() - mLastShiftTimeMSec) / 1000.0 > mMaxShiftInterval)
            mShifts.clear();

        T result = calculateSpeed(mShifts);
        mShifts.clear();

        return result;
    }

    /**
     * This function calculates the magnitude of the given value.
     * 
     * By default, it uses the appropriate free-standing 
     * <tt>calculateMagnitude</tt> function picked by ADL.
     * 
     * It can be reimplemented in derived class if non-standard magnitude
     * calculation is desired.
     * 
     * \param value                     Value to calculated magnitude of.
     * \returns                         Magnitude of the given value.
     */
    virtual qreal magnitude(const T &value) const {
        using detail::calculateMagnitude;

        return calculateMagnitude(value);
    }   

    /**
     * This functions implements deceleration of the kinetic movement over time.
     * 
     * Default implementation uses friction value as deceleration.
     * 
     * \param initialSpeed              Speed at the beginning of kinetic process.
     * \param currentSpeed              Current speed.
     * \param speedGain                 Speed gain due to new shifts added since the 
     *                                  last call to this function.
     * \param dt                        Time since the last call to this function, in seconds.
     * \returns                         New speed.
     */
    virtual T updateSpeed(const T &initialSpeed, const T &currentSpeed, const T &speedGain, qreal dt) const {
        unused(initialSpeed);

        T speed = currentSpeed + speedGain;

        qreal oldMagnitude = magnitude(speed);
        qreal newMagnitude = oldMagnitude - mFriction * dt;
        if(newMagnitude <= 0.0)
            return T();

        return speed * (newMagnitude / oldMagnitude);
    }

    virtual void tick(int currentTimeMSec) override {
        /* Update current speed. */
        T speedGain = calculateSpeed();
        qreal dt = (currentTimeMSec - mLastTickTimeMSec) / 1000.0;
        mCurrentSpeed = updateSpeed(mInitialSpeed, mCurrentSpeed, speedGain, dt);

        /* Adjust for max magnitude. */
        qreal currentSpeedMagnitude = magnitude(mCurrentSpeed);
        if(currentSpeedMagnitude > mMaxSpeedMagnitude)
            mCurrentSpeed = mCurrentSpeed * (mMaxSpeedMagnitude / currentSpeedMagnitude);

        qDebug() << mCurrentSpeed;

        mLastTickTimeMSec = currentTimeMSec;

        if(magnitude(mCurrentSpeed) <= mMinSpeedMagnitude) {
            reset();
        } else {
            if(!qFuzzyIsNull(dt) && mHandler != NULL)
                mHandler->moveKinetic(mCurrentSpeed * dt);
        }
    }

  private:
    void transition(State state) {
        if(mState == state)
            return;

        mState = state;

        switch(state) {
        case MEASURING: /* KINETIC -> MEASURING. */
            mTimer->stop();
            if(mHandler != NULL)
                mHandler->finishKinetic();
            return;
        case KINETIC: /* MEASURING -> KINETIC. */
            mInitialSpeed = mCurrentSpeed = calculateSpeed();
            mLastTickTimeMSec = 0;
            mTimer->setCurrentTime(0);
            if(mState == state) { /* State may get changed in a callback. */
                mTimer->start();
                if(mState == state && mHandler != NULL) /* And again, state may have changed. */
                    mHandler->startKinetic();
            }
            return;
        default:
            return;
        }
    }

  private:
    /* 'Working' state. */

    /** Current state. */
    State mState;

    /** Queue of recorded shifts. */
    ShiftList mShifts;

    /** Last time a shift was recorded. */
    qint64 mLastShiftTimeMSec;

    /** Last time that was passed to tick(). */
    int mLastTickTimeMSec;

    /** Speed at the beginning of kinetic motion. */
    T mInitialSpeed;

    /** Current kinetic speed. */
    T mCurrentSpeed;


    /* 'Stable' state. */

    /** Timer that is synced with other animations. */
    AnimationTimer *mTimer;

    /** Kinetic handler. */
    KineticProcessHandler<T> *mHandler;

    /** Maximal size of the shift queue. */
    int mMaxShiftCount;

    /** Maximal time interval between consequent shifts, in seconds. */
    qreal mMaxShiftInterval;

    /** Minimal speed magnitude. */
    qreal mMinSpeedMagnitude;

    /** Maximal speed magnitude. */
    qreal mMaxSpeedMagnitude;

    /** Friction coefficient. */
    qreal mFriction;
};


template<class T>
KineticProcessHandler<T>::~KineticProcessHandler() {
    if(mProcessor != NULL)
        mProcessor->setHandler(NULL);
}


#endif // QN_KINETIC_PROCESSOR_H
