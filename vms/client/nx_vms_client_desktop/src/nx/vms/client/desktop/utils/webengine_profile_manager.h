#include <QtCore/QObject>
#include <QtWebEngine/QQuickWebEngineProfile>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop::utils {

/**
 * QML singleton that provides unique QQuickWebEngineProfile instance by its name.
 * Each VMS user has its own profile name (based on user UUID) to avoid unauthorized access
 * to websites opened during another user session.
 * User profiles store their data on disk to allow restoring of login sessions for web pages.
 * When there is no current user (example: system setup wizard) an off-the-record profile is used
 * so nothing is stored on disk.
 */
class NX_VMS_CLIENT_DESKTOP_API WebEngineProfileManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    WebEngineProfileManager();
    virtual ~WebEngineProfileManager() override;

    WebEngineProfileManager(const WebEngineProfileManager&) = delete;
    WebEngineProfileManager& operator=(const WebEngineProfileManager&) = delete;

public:
    static WebEngineProfileManager* instance();

    /** Get WebEngine profile based on its name. Same instance for the same name is returned. */
    Q_INVOKABLE QQuickWebEngineProfile* getProfile(const QString& name);

    static void registerQmlType();

signals:
    void downloadRequested(QObject* download);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::utils
