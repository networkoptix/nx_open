#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/delegates/abstract_permissions_delegate.h>
#include <ui/workbench/workbench_context_aware.h>

class QnUserSettingsModel : public QObject, public QnWorkbenchContextAware, public QnAbstractPermissionsDelegate
{
    Q_OBJECT

    typedef QObject base_type;
public:
    /** User editing mode */
    enum Mode
    {
        Invalid,        /**< User is not set. */
        NewUser,        /**< Creating new user. */
        OwnProfile,     /**< Editing own profile. */
        OtherProfile,   /**< Viewing public profile of another user, which we cannot edit. */
        OtherSettings   /**< Edit other user settings */
    };

    QnUserSettingsModel(QObject* parent = nullptr);
    virtual ~QnUserSettingsModel();

    /** Current working mode. */
    Mode mode() const;

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr& value);

    virtual Qn::GlobalPermissions rawPermissions() const override;
    virtual void setRawPermissions(Qn::GlobalPermissions value) override;

    virtual QSet<QnUuid> accessibleResources() const override;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) override;
private:
    Mode m_mode;
    QnUserResourcePtr m_user;
};
