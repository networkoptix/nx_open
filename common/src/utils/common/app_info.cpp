#include "app_info.h"

QnAppInfo::QnAppInfo(QObject* parent):
    QObject(parent)
{
}

bool QnAppInfo::isArm()
{
    return applicationArch() == lit("arm");
}

bool QnAppInfo::isBpi()
{
    return armBox() == lit("bpi");
}

bool QnAppInfo::isNx1()
{
    return armBox() == lit("nx1");
}

bool QnAppInfo::isAndroid()
{
    return applicationPlatform() == lit("android");
}

bool QnAppInfo::isIos()
{
    return applicationPlatform() == lit("ios");
}

bool QnAppInfo::isMobile()
{
    return isAndroid() || isIos();
}

QString QnAppInfo::applicationFullVersion()
{
    static const QString kBeta = beta() ? lit("-beta") : QString();
    static const QString kFullVersion = lit("%1-%2-%3%4")
        .arg(applicationVersion())
        .arg(applicationRevision())
        .arg(customizationName().replace(L' ', L'_'))
        .arg(kBeta);

    return kFullVersion;
}
