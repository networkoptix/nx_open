#ifndef QN_INSTALLATION_MODE_H
#define QN_INSTALLATION_MODE_H

#include <QList>

class Instrument;

class InstallationMode {
public:
    enum Mode {
        INSTALL_LAST,   /**< Install after all present instruments. */
        INSTALL_FIRST,  /**< Install before all present instruments. */
        INSTALL_BEFORE, /**< Install before the given instrument. If NULL is given or given instrument not found, inserts at the end. */
        INSTALL_AFTER   /**< Install after the given instrument. If NULL is given or given instrument not found, inserts at the beginning. */
    };

protected:
    static void insertInstrument(Instrument *instrument, Mode mode, Instrument *reference, QList<Instrument *> *target);
};


#endif QN_INSTALLATION_MODE_H
