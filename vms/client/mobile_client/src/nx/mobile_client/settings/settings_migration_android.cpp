#include "settings_migration.h"

#ifdef Q_OS_ANDROID

#include <QtAndroidExtras/QAndroidJniObject>

#include <mobile_client/mobile_client_app_info.h>
#include <mobile_client/mobile_client_settings.h>

namespace nx {
namespace mobile_client {
namespace settings {

QnMigratedSessionList getMigratedSessions(bool *success)
{
    QnMigratedSessionList result;

    if (success)
        *success = false;

    QString uriString = lit("content://") +
        QnMobileClientAppInfo::oldAndroidAppId() +
                        lit(".LogonEntries/#HereWillBeDragonsRazRazRaz111!!!");

    QAndroidJniObject uri = QAndroidJniObject::callStaticObjectMethod(
                "android/net/Uri", "parse",
                "(Ljava/lang/String;)Landroid/net/Uri;",
                QAndroidJniObject::fromString(uriString).object<jstring>());
    if (!uri.isValid())
        return result;

    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod(
                "org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (!activity.isValid())
        return result;

    QAndroidJniObject contentResolver = activity.callObjectMethod(
                "getContentResolver", "()Landroid/content/ContentResolver;");
    if (!contentResolver.isValid())
        return result;

    QAndroidJniObject cursor = contentResolver.callObjectMethod(
                "query",
                "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
                uri.object<jobject>(), nullptr, nullptr, nullptr, nullptr);
    if (!cursor.isValid())
        return result;

    enum class Field
    {
        Title,
        Login,
        Password,
        Host,
        Port
    };

    while (cursor.callMethod<jboolean>("moveToNext"))
    {
        QnMigratedSession session;

        session.title = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", Field::Title).toString();
        session.login = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", Field::Login).toString();
        session.password = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", Field::Password).toString();
        session.host = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", Field::Host).toString();
        session.port = cursor.callMethod<jint>("getInt", "(I)I", Field::Port);

        result.append(session);
    }

    if (success)
        *success = true;

    return result;
}

} // namespace settings
} // namespace mobile_client
} // namespace nx

#endif
