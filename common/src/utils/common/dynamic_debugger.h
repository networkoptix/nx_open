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

/**
 * Macro for convinient QnDynamicDebugger usage.
 * Sample: DYNAMIC_DEBUG(this, SIGNAL(geometryChanged()), "geometry changed");
*/
#define DYNAMIC_DEBUG(obj, signal, text) QObject::connect(obj, signal, new QnDynamicDebugger(QLatin1String(text), obj), SLOT(print()), Qt::DirectConnection)

#endif // DYNAMIC_DEBUGGER_H
