#ifndef QN_GL_HARDWARE_CHECKER_H
#define QN_GL_HARDWARE_CHECKER_H

#include <QtCore/QCoreApplication>

class QnGlHardwareChecker {
    Q_DECLARE_TR_FUNCTIONS(QnGlHardwareChecker)
public:
    static bool checkCurrentContext(bool displayWarnings);
};

#endif // QN_GL_HARDWARE_CHECKER_H
