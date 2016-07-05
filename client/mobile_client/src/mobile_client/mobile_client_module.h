#pragma once

#include <QtCore/QObject>

class QnMobileClientModule : public QObject {
    Q_OBJECT
public:
    QnMobileClientModule(QObject *parent = NULL);
    ~QnMobileClientModule();
};
