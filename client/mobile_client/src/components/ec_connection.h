#ifndef EC_CONNECTION_H
#define EC_CONNECTION_H

#include <QObject>

#include <context/context_aware.h>

class EcConnection : public QObject, public QnContextAware {
    Q_OBJECT

public:
    explicit EcConnection(QObject *parent = 0);

    Q_INVOKABLE void connect(const QUrl &url);

signals:
    void connected();


};

#endif // EC_CONNECTION_H
