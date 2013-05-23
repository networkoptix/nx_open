#ifndef DYNAMIC_DEBUGGER_H
#define DYNAMIC_DEBUGGER_H

#include <QObject>

class QnDynamicDebugger: public QObject {
    Q_OBJECT
public:
    QnDynamicDebugger(const QString &debugString, QObject *parent = NULL):
        QObject(parent), m_debugString(debugString){}
    virtual ~QnDynamicDebugger() {}

public slots:
    void print() {
        qDebug() << m_debugString;
    }

private:
    QString m_debugString;
};

#endif // DYNAMIC_DEBUGGER_H
