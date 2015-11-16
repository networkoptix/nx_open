#ifndef QNMOBILEAPPINFO_H
#define QNMOBILEAPPINFO_H

#include <QtCore/QObject>

class QnMobileAppInfo : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString productName READ productName NOTIFY productNameChanged)
    Q_PROPERTY(QString organizationName READ organizationName NOTIFY organizationNameChanged)

public:
    explicit QnMobileAppInfo(QObject *parent = 0);

    QString productName() const;
    QString organizationName() const;

signals:
    void productNameChanged();
    void organizationNameChanged();
};

#endif // QNMOBILEAPPINFO_H
