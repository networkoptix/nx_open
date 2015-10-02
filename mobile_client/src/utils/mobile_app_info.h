#ifndef QNMOBILEAPPINFO_H
#define QNMOBILEAPPINFO_H

#include <QtCore/QObject>

class QnMobileAppInfo : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString productName READ productName NOTIFY productNameChanged)
public:
    explicit QnMobileAppInfo(QObject *parent = 0);

    QString productName() const;

signals:
    void productNameChanged();
};

#endif // QNMOBILEAPPINFO_H
