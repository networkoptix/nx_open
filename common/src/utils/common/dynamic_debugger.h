#ifndef DYNAMIC_DEBUGGER_H
#define DYNAMIC_DEBUGGER_H

#include <QDebug>
#include <QObject>

class QnDynamicDebugger: public QObject {
    Q_OBJECT
public:
    QnDynamicDebugger(const QString &debugString, QObject *parent = NULL):
        QObject(parent), m_debugString(debugString){}
    virtual ~QnDynamicDebugger() {}

public slots:
    void print() {
#ifndef Q_OS_MAC
        qDebug() << m_debugString;
#endif
    }

private:
    QString m_debugString;
};

#endif // DYNAMIC_DEBUGGER_H
