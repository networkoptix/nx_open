// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS
#include <QtGui/QValidator>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLayout>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/shared_resource_pointer.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/event/event_fwd.h>
#include <ui/delegates/resource_selection_dialog_delegate.h>

class QnResourceAccessSubject;

namespace QnBusiness {

bool actionAllowedForUser(
    const nx::vms::event::AbstractActionPtr& action,
    const QnUserResourcePtr& user);

} // namespace QnBusiness

struct QnAllowAnyCameraPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnAllowAnyCameraPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr&) { return true; }
    static QString getText(const QnResourceList& resources, const bool detailed = true);
    static bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
};

struct QnRequireCameraPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnRequireCameraPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr&) { return true; }
    static QString getText(const QnResourceList& resources, const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
};

class QnCameraInputPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraInputPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
};

class QnCameraOutputPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraOutputPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
};

class QnExecPtzPresetPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnExecPtzPresetPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return false; }
    static bool showRecordingIndicator() { return false; }
    static bool canUseSourceCamera() { return false; }
};

class QnCameraMotionPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraMotionPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return true; }
};

class QnCameraAudioTransmitPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraAudioTransmitPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
};

class QnCameraRecordingPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraRecordingPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return true; }
};

class QnCameraAnalyticsPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraAnalyticsPolicy)
public:
    using resource_type = QnVirtualCameraResource;
    static bool isResourceValid(const QnVirtualCameraResourcePtr& camera);
    static QString getText(const QnResourceList& resources, const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
    static bool showRecordingIndicator() { return false; }
};

using QnBookmarkActionPolicy = QnCameraRecordingPolicy;

class QnFullscreenCameraPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnFullscreenCameraPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return false; }
    static bool showRecordingIndicator() { return false; }
    static bool canUseSourceCamera() { return true; }
};

template<typename CheckingPolicy>
static bool isResourcesListValid(const QnResourceList& resources)
{
    typedef typename CheckingPolicy::resource_type ResourceType;

    auto filtered = resources.filtered<ResourceType>();

    if (filtered.isEmpty())
        return CheckingPolicy::emptyListIsValid();

    if (filtered.size() > 1 && !CheckingPolicy::multiChoiceListIsValid())
        return false;

    for (const auto& resource: filtered)
        if (!CheckingPolicy::isResourceValid(resource))
            return false;
    return true;
}

class QnSendEmailActionDelegate: public QnResourceSelectionDialogDelegate
{
    Q_DECLARE_TR_FUNCTIONS(QnSendEmailActionDelegate)
public:
    QnSendEmailActionDelegate(QWidget* parent);
    ~QnSendEmailActionDelegate() {}

    void init(QWidget* parent) override;

    QString validationMessage(const QSet<QnUuid>& selected) const override;
    bool isValid(const QnUuid& resourceId) const override;

    static bool isValidList(const QSet<QnUuid>& ids, const QString& additional);

    static QString getText(const QSet<QnUuid>& ids, const bool detailed,
        const QString& additional);
private:
    static QStringList parseAdditional(const QString& additional);
    static bool isValidUser(const QnUserResourcePtr& user);
private:
    QLabel* m_warningLabel;
};

// User and role validation policy.
class QnSubjectValidationPolicy: public nx::vms::client::core::CommonModuleAware
{
public:
    explicit QnSubjectValidationPolicy(bool allowEmptySelection = false);
    virtual ~QnSubjectValidationPolicy();

    virtual QValidator::State roleValidity(const QnUuid& roleId) const = 0;
    virtual bool userValidity(const QnUserResourcePtr& user) const = 0;

    virtual QValidator::State validity(bool allUsers, const QSet<QnUuid>& subjects) const;
    virtual QString calculateAlert(bool allUsers, const QSet<QnUuid>& subjects) const;

    void analyze(bool allUsers, const QSet<QnUuid>& subjects,
        QVector<QnUuid>& validRoles,
        QVector<QnUuid>& invalidRoles,
        QVector<QnUuid>& intermediateRoles,
        QVector<QnUserResourcePtr>& validUsers,
        QVector<QnUserResourcePtr>& invalidUsers) const;

    bool isEmptySelectionAllowed() const;

private:
    const bool m_allowEmptySelection;
};

// Default validation policy: users and roles are always valid.
class QnDefaultSubjectValidationPolicy: public QnSubjectValidationPolicy
{
    using base_type = QnSubjectValidationPolicy;

public:
    QnDefaultSubjectValidationPolicy(bool allowEmptySelection = false);

    virtual QValidator::State roleValidity(const QnUuid& roleId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;
};

// Subject validation policy when one global permission is required.
class QnRequiredAccessRightPolicy: public QnSubjectValidationPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnRequiredAccessRightPolicy)
    using base_type = QnSubjectValidationPolicy;

public:
    explicit QnRequiredAccessRightPolicy(
        nx::vms::api::AccessRight requiredAccessRight,
        bool allowEmptySelection = false);

    virtual QValidator::State roleValidity(const QnUuid& groupId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;
    virtual QString calculateAlert(bool allUsers, const QSet<QnUuid>& subjects) const override;

    QnSharedResourcePointerList<QnVirtualCameraResource> cameras() const;
    void setCameras(const QnSharedResourcePointerList<QnVirtualCameraResource>& cameras);

private:
    bool isSubjectValid(
        const QnResourceAccessSubject& subject, bool allSelectedCamerasRequired = true) const;
    bool hasAnyCameraPermission(const QnUserResourcePtr& user) const;

private:
    const nx::vms::api::AccessRight m_requiredAccessRight;
    QnSharedResourcePointerList<QnVirtualCameraResource> m_cameras;
};

class QnLayoutAccessValidationPolicy: public QnSubjectValidationPolicy
{
    using base_type = QnSubjectValidationPolicy;

public:
    virtual QValidator::State roleValidity(const QnUuid& roleId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;

    void setLayout(const QnLayoutResourcePtr& layout);

private:
    QnLayoutResourcePtr m_layout;
};

class QnCloudUsersValidationPolicy: public QnSubjectValidationPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCloudUsersValidationPolicy)
    using base_type = QnSubjectValidationPolicy;

public:
    virtual QValidator::State roleValidity(const QnUuid& roleId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;
    virtual QString calculateAlert(bool allUsers, const QSet<QnUuid>& subjects) const override;
};

class QnUserWithEmailValidationPolicy: public QnSubjectValidationPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnUsersWithEmailValidationPolicy)
    using base_type = QnSubjectValidationPolicy;

public:
    virtual QValidator::State roleValidity(const QnUuid& roleId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;
};

class QnBuzzerPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnBuzzerPolicy)
public:
    static bool isServerValid(const QnMediaServerResourcePtr& server);
    static QString infoText();
};

class QnPoeOverBudgetPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnPoeOverBudgetPolicy)
public:
    static bool isServerValid(const QnMediaServerResourcePtr& server);
    static QString infoText();
};

class QnFanErrorPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnFanErrorPolicy)
public:
    static bool isServerValid(const QnMediaServerResourcePtr& server);
    static QString infoText();
};
