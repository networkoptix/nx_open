#include "intent_listener_android.h"

#if defined(Q_OS_ANDROID)

#include <QtGui/QDesktopServices>
#include <QtAndroidExtras/QAndroidJniObject>
#include <private/qjnihelpers_p.h>

class QnIntentListener : public QtAndroidPrivate::NewIntentListener
{
    virtual bool handleNewIntent(JNIEnv* env, jobject intentObject) override
    {
        Q_UNUSED(env);

        QAndroidJniObject intent(intentObject);

        QAndroidJniObject uri = intent.callObjectMethod("getData", "()Landroid/net/Uri;");
        if (!uri.isValid())
            return false;

        QString uriString = uri.toString();
        if (uriString.isEmpty())
            return false;

        QUrl url(uriString);
        if (!url.isValid())
            return false;

        qDebug() << "Opening URI" << uriString;

        return QDesktopServices::openUrl(url);
    }
};

void registerIntentListener()
{
    QtAndroidPrivate::registerNewIntentListener(new QnIntentListener());
}

#endif // defined(Q_OS_ANDROID)
