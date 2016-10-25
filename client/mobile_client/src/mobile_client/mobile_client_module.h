#pragma once

#include <QtCore/QObject>

struct QnMobileClientStartupParameters;

class QnMobileClientModule: public QObject
{
public:
    QnMobileClientModule(
        const QnMobileClientStartupParameters& startupParameters,
        QObject* parent = nullptr);
    ~QnMobileClientModule();
};
