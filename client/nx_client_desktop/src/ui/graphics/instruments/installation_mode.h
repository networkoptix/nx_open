#ifndef QN_INSTALLATION_MODE_H
#define QN_INSTALLATION_MODE_H

#include <QtCore/QList>

class Instrument;

class InstallationMode {
public:
    enum Mode {
        /** Installed instrument will be the first in event processing queue. */
        InstallFirst,  

        /** Installed instrument will be the last in event processing queue. */
        InstallLast,   

        /** Installed instrument will be placed before the given instrument in event processing queue. 
         * If NULL is given or given reference not found, it will be placed first in the queue. */
        InstallBefore, 

        /** Installed instrument will be placed after the given instrument in event processing queue. 
         * If NULL is given or given reference not found, it will be placed last in the queue. */
        InstallAfter,  

        InstallationModeCount
    };

protected:
    static void insertInstrument(Instrument *instrument, Mode mode, Instrument *reference, QList<Instrument *> *target);
};


#endif // QN_INSTALLATION_MODE_H
