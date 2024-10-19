// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intent_listener_android.h"

#if defined(Q_OS_ANDROID)

#include <QtCore/QDebug>
#include <QtCore/QJniObject>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtGui/QDesktopServices>

namespace {

QUrl getUrlFromIntent(QJniObject intent)
{
    QJniObject uri = intent.callObjectMethod("getData", "()Landroid/net/Uri;");
    if (!uri.isValid())
        return QUrl();

    QString uriString = uri.toString();
    if (uriString.isEmpty())
        return QUrl();

    QUrl url(uriString);
    if (!url.isValid())
        return QUrl();

    return url;
}

} // namespace

class QnIntentListener : public QtAndroidPrivate::NewIntentListener
{
    virtual bool handleNewIntent(JNIEnv* env, jobject intentObject) override
    {
        Q_UNUSED(env);

        QUrl url = getUrlFromIntent(intentObject);
        if (!url.isValid())
            return false;

        qDebug() << "Opening URI" << url;

        return QDesktopServices::openUrl(url);
    }
};

void registerIntentListener()
{
    QtAndroidPrivate::registerNewIntentListener(new QnIntentListener());
}

QUrl getInitialIntentData()
{
    QJniObject intent = QJniObject(QtAndroidPrivate::activity()).callObjectMethod(
        "getIntent", "()Landroid/content/Intent;");

    if (!intent.isValid())
        return QUrl();

    return getUrlFromIntent(intent);
}

#endif // defined(Q_OS_ANDROID)
