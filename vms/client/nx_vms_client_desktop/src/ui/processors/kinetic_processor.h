#ifndef QN_KINETIC_PROCESSOR_H
#define QN_KINETIC_PROCESSOR_H

#include <limits>

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtCore/QDateTime>

#include <ui/animation/animation_timer_listener.h>
#include <utils/math/magnitude.h>
#include <utils/math/linear_combination.h>

#include "kinetic_process_handler.h"

/**
 * Kinetic processor implements logic that is common to components that make
 * use of kinetic motion.
 * 
 * Kinetic processor presents several functions that are to be used from user
 * code. The most important functions are:
 * <ul>
 * <li><tt>reset()</tt>, which is to be used when a user-controlled motion starts, 
 *    or when there is a need to stop kinetic process prematurely. </li>
 * <li><tt>shift()</tt>, which is be to used to feed position delta
 *    values to kinetic processor while user-controller motion is in progress.</tt>
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
class KineticProcessor : public QObject, public AnimationTimerListener {
    Q_OBJECT;
    Q_FLAGS(Flags Flag);
    Q_ENUMS(State);

public:
    enum Flag {
        /** When this flag is set, delta time between events is ignored and
         * all events are treated as being separated by a constant amount of 
         * time determined by <tt>maxShiftInterval</tt>. */
        IgnoreDeltaTime = 0x1 // TODO: #Elric #enum
    };

    Q_DECLARE_FLAGS(Flags, Flag);

    /**
     * Kinetic state.
     */
    enum State {
        Measuring, /**< Recording shifts. */
        Running    /**< Performing kinetic motion. */
    };

    enum { // TODO: #Elric #enum
        DEFAULT_MAX_SHIFT_COUNT = 10,
        DEFAULT_MAX_SHIFT_INTERVAL_MSEC = 200
    };

    /**
     * Constructor.
     * 
     * \param type                      <tt>QMetaType::Type</tt> for spatial type of this processor.
     * \param parent                    Parent for this object.
     */
    KineticProcessor(int type, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~KineticProcessor();

    /**
     * Resets this kinetic processor into its initial state.
     */
    void reset();

    /**
     * \param dv                        Spatial displacement since the last call to this function.
     */
    void shift(const QVariant &dv);

    /**
     * Starts kinetic process.
     */
    void start() {
        transition(Running);
    }

    /**
     * \returns                         <tt>QMetaType::Type</tt> for spatial type of this processor.
     */
    int type() const {
        return mType;
    }

    /**
     * \returns                         Flags of this kinetic processor. 
     */
    Flags flags() const {
        return mFlags;
    }

    /**
     * \param flags                     New flags for this kinetic processor. 
     */
    void setFlags(Flags flags) {
        mFlags = flags;
    }

    /**
     * \returns                         Current state of this kinetic processor.
     */
    State state() const {
        return mState;
    }

    /**
     * \returns                         Whether this processor is currently measuring offsets to
     *                                  start a kinetic motion.
     */
    bool isMeasuring() const {
        return mState == Measuring;
    }

    /**
     * \returns                         Whether this processor is currently performing a kinetic motion.
     */
    bool isRunning() const {
        return mState == Running;
    }

    /**
     * \returns                         Handler for this kinetic processor.
     */
    KineticProcessHandler *handler() const {
        return mHandler;
    }

    /**
     * \param handler                   New handler for this kinetic processor.
     */
    void setHandler(KineticProcessHandler *handler);
    
    /**
     * \param count                     Maximal number of latest shifts that will
     *                                  be considered when computing initial
     *                                  kinetic speed. Good values are in range [5, 15].
     */
    void setMaxShiftCount(int count);

    int maxShiftCount() const {
        return mMaxShiftCount;
    }

    /**
     * \param minSpeedMagnitude         Minimal speed magnitude. When current speed 
     *                                  magnitude becomes less or equal to this value,
     *                                  kinetic motion stops.
     */
    void setMinSpeedMagnitude(qreal minSpeedMagnitude);

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
    void setFriction(qreal friction);

    qreal friction() const {
        return mFriction;
    }

    QVariant currentSpeed() const {
        return mCurrentSpeed;
    }

protected:
    struct Shift {
        QVariant dv; /**< Spatial displacement. */
        qreal dt;    /**< Time delta, in seconds. */
    };

    typedef QList<Shift> ShiftList;

    MagnitudeCalculator *magnitudeCalculator() const {
        return mMagnitudeCalculator;
    }

    LinearCombinator *linearCombinator() const {
        return mLinearCombinator;
    }

    /**
     * Given a valid list of shifts, this function calculates speed.
     * 
     * Default implementation just takes the mean. If some other behavior is 
     * desired, this function can be reimplemented in derived class.
     * 
     * \param shifts                    List of shifts.
     * \returns                         Estimated speed, in T per second.
     */
    virtual QVariant calculateSpeed(const ShiftList &shifts) const;

    /**
     * Calculates speed based on currently recorded shifts and clears them.
     */
    QVariant calculateSpeed();

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
    virtual QVariant updateSpeed(const QVariant &initialSpeed, const QVariant &currentSpeed, const QVariant &speedGain, qreal dt) const;

    virtual void tick(int deltaTimeMSec) override;

  private:
    void transition(State state);

  private:
    /* 'Working' state. */

    /** Current state. */
    State mState;

    /** Queue of recorded shifts. */
    ShiftList mShifts;

    /** Last time a shift was recorded. */
    qint64 mLastShiftTimeMSec;

    /** Speed at the beginning of kinetic motion. */
    QVariant mInitialSpeed;

    /** Current kinetic speed. */
    QVariant mCurrentSpeed;


    /* 'Stable' state. */

    /** <tt>QMetaType::Type</tt> for the spatial type of this processor. */
    int mType;

    /** Magnitude calculator. */
    MagnitudeCalculator *mMagnitudeCalculator;

    /** Linear combinator. */
    LinearCombinator *mLinearCombinator;

    /** Flags */
    Flags mFlags;

    /** Kinetic handler. */
    KineticProcessHandler *mHandler;

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

#endif // QN_KINETIC_PROCESSOR_H
