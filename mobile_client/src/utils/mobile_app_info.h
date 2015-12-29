#ifndef QNMOBILEAPPINFO_H
#define QNMOBILEAPPINFO_H

#include <QtCore/QObject>

class QnMobileAppInfo : public QObject {
    Q_OBJECT

public:
    explicit QnMobileAppInfo(QObject *parent = 0);

    Q_INVOKABLE QString productName() const;
    Q_INVOKABLE QString organizationName() const;

    Q_INVOKABLE QUrl oldMobileClientUrl() const;
};

#endif // QNMOBILEAPPINFO_H
