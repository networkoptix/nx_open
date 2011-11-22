#ifndef QN_INSTALLATION_MODE_H
#define QN_INSTALLATION_MODE_H

class InstallationMode {
public:
    enum Mode {
        INSTALL_LAST,   /**< Install after all present instruments. */
        INSTALL_FIRST   /**< Install before all present instruments. */
    };

protected:
    static void insertInstrument(Instrument *instrument, Mode mode, QList<Instrument *> *target);
};


#endif QN_INSTALLATION_MODE_H
