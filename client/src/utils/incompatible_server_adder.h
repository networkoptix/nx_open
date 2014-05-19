#ifndef INCOMPATIBLE_SERVER_ADDER_H
#define INCOMPATIBLE_SERVER_ADDER_H

#include <QtCore/QObject>

class QnModuleInformation;

class QnIncompatibleServerAdder : public QObject {
    Q_OBJECT
public:
    explicit QnIncompatibleServerAdder(QObject *parent = 0);

signals:

private slots:
    void at_peerFound(const QnModuleInformation &moduleInformation);
    void at_peerChanged(const QnModuleInformation &moduleInformation);
    void at_peerLost(const QnModuleInformation &moduleInformation);
};

#endif // INCOMPATIBLE_SERVER_ADDER_H
