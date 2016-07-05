#ifndef QN_KINETIC_PROCESS_HANDLER_H
#define QN_KINETIC_PROCESS_HANDLER_H

#include <QtCore/QtGlobal> /* For Q_UNUSED. */

class QVariant;

class KineticProcessor;

/**
 * This interface is to be implemented by the receiver of kinetic events.
 * 
 * It is to be used in conjunction with <tt>KineticProcessor</tt>.
 */
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
    virtual void kineticMove(const QVariant &distance) { Q_UNUSED(distance); };

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
    KineticProcessor *kineticProcessor() const {
        return mProcessor;
    }

private:
    friend class KineticProcessor;

    KineticProcessor *mProcessor;
};


#endif // QN_KINETIC_PROCESS_HANDLER_H
