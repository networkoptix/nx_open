#pragma once

#include <QtCore/QObject>

class QnMobileClientStartupParameters;

class QnMobileClientModule: public QObject
{
public:
    QnMobileClientModule(
        const QnMobileClientStartupParameters& startupParameters,
        QObject* parent = nullptr);
    ~QnMobileClientModule();
};
