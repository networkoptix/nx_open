#include "AxHDWitness.h"

#include <utils/common/sleep.h>

#include <QAxFactory>
#include <QApplication>
#include <QRunnable>
#include <QThreadPool>


// namespace hdw {
QAXFACTORY_BEGIN("{817D1E50-BFBA-4691-B331-1B8AA838A745}", "{49D52858-E6F3-441A-A5F7-24A3B3E056AF}")
    QAXCLASS(AxHDWitness)
QAXFACTORY_END()
// } // namespace hdw


// ### Qt5: remove this hack, the bug is fixed there.
/*
 * Yet another evil hack, this time to work around a crash in QVariant.
 * 
 * When loaded, QtGui replaces QtCore's QVariant handler with its own. 
 * It stores the old handler in a static variable, so that it can be restored
 * when QtGui is unloaded.
 * 
 * The only problem is that QtGui replaces the handler TWICE - once from static
 * initializer, and once from QApplication constructor, overwriting the static 
 * pointer to the old handler with QtGui's handler, and thus ending up in a 
 * situation when it is not possible to properly restore the old handler.
 * 
 * If there is some QVariant usage in QtCore static deinitializers, then we're 
 * in trouble. So we hack.
 */
Q_CORE_EXPORT const QVariant::Handler *qcoreVariantHandler();

QApplication *qn_application = NULL;
static int qn_argc = 0; /* This one must be static as QApplication stores a reference to it. */
static char **qn_argv;

#if 0
QAxFactory *qax_instantiate() {
    QAxFactory *result = hdw::qax_instantiate();

    if(qApp) {
        /* QApplication instance must not exist for the hack to work. */
        fprintf(stderr, "ERROR: Failed to load HD Witness uncrash hack!\n");
    } else {
/*        const QVariant::Handler **currentHandler = QnHackedVariant::handler();
        const QVariant::Handler *oldHandler = *currentHandler;
        *currentHandler = qcoreVariantHandler();*/
        
		LPWSTR *szArgsList = CommandLineToArgvW(GetCommandLineW(), &qn_argc);
		qn_argv = new char*[qn_argc];
		for (int i = 0; i < qn_argc; ++i) {
			qn_argv[i] = "";
		}

		qn_application = new QApplication(qn_argc, qn_argv);

        /* Roll back if QApplication constructor didn't behave as expected. */
        /* if(*currentHandler == qcoreVariantHandler()) {
            fprintf(stderr, "ERROR: Something got messed up in HD Witness uncrash hack!\n");
            *currentHandler = oldHandler;
        }*/
    }

    return result;
}

void qax_uninstantiate() {
    if(!qApp || qApp != qn_application)
        return;

    //ExitProcess(0);
    //terminate();

    //delete qn_application;
    //qn_application = NULL;
}

Q_DESTRUCTOR_FUNCTION(qax_uninstantiate);

#endif
