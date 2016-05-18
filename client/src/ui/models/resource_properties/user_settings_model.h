#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/models/abstract_permissions_model.h>
#include <ui/workbench/workbench_context_aware.h>

class QnUserSettingsModel : public QObject, public QnAbstractPermissionsModel, public QnWorkbenchContextAware
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

    QString groupName() const;

    /** Return human-readable permissions description for the target user */
    QString permissionsDescription() const;

    /** Return human-readable permissions description for the selected permissions set. */
    QString permissionsDescription(const QnUserResourcePtr& user, const QnUuid& groupId) const;

private:
    QString getCustomPermissionsDescription(const QnUuid& id, Qn::GlobalPermissions permissions) const;

private:
    Mode m_mode;
    QnUserResourcePtr m_user;
};
