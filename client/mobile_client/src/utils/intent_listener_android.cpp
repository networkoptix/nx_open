#include "intent_listener_android.h"

#if defined(Q_OS_ANDROID)

#include <QtGui/QDesktopServices>
#include <QtAndroidExtras/QtAndroid>
#include <private/qjnihelpers_p.h>

namespace {

QUrl getUrlFromIntent(QAndroidJniObject intent)
{
    QAndroidJniObject uri = intent.callObjectMethod("getData", "()Landroid/net/Uri;");
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
    QAndroidJniObject intent = QtAndroid::androidActivity().callObjectMethod(
        "getIntent", "()Landroid/content/Intent;");

    if (!intent.isValid())
        return QUrl();

    return getUrlFromIntent(intent);
}

#endif // defined(Q_OS_ANDROID)
