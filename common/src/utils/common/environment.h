#ifndef QN_ENVIRONMENT_H
#define QN_ENVIRONMENT_H

#include <QtCore/QObject>

class QnEnvironment: public QObject {
    Q_OBJECT;
public:
    static QString searchInPath(QString executable);

    static void showInGraphicalShell(QWidget *parent, const QString &path);
};


#endif // QN_ENVIRONMENT_H
