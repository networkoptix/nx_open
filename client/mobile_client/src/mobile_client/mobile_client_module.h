#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>

class QnCommonModule;
struct QnMobileClientStartupParameters;

class QnMobileClientModule: public QObject, public Singleton<QnMobileClientModule>
{
public:
    QnMobileClientModule(
        const QnMobileClientStartupParameters& startupParameters,
        QObject* parent = nullptr);
    ~QnMobileClientModule();

    QnCommonModule* commonModule() const;

private:
    QnCommonModule* m_commonModule;
};

#define qnMobileClientModule QnMobileClientModule::instance()
