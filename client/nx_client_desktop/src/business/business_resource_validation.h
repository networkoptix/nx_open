#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <QtWidgets/QLayout>
#include <QtWidgets/QLabel>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/shared_resource_pointer.h>
#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/style/custom_style.h>

#include <nx/vms/event/event_fwd.h>

namespace QnBusiness {

bool actionAllowedForUser(const nx::vms::event::ActionParameters& params,
    const QnUserResourcePtr& user);

bool hasAccessToSource(const nx::vms::event::EventParameters& params,
    const QnUserResourcePtr& user);

} // namespace QnBusiness

class QnCameraInputPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraInputPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
};

class QnCameraOutputPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraOutputPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
};

class QnExecPtzPresetPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnExecPtzPresetPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return false; }
};

class QnCameraMotionPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraMotionPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return true; }
    static bool multiChoiceListIsValid() { return true; }
};

class QnCameraAudioTransmitPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraAudioTransmitPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
};

class QnCameraRecordingPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraRecordingPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getText(const QnResourceList &resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
};

class QnCameraAnalyticsPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCameraAnalyticsPolicy)
public:
    using resource_type = QnVirtualCameraResource;
    static bool isResourceValid(const QnVirtualCameraResourcePtr& camera);
    static QString getText(const QnResourceList& resources, const bool detailed = true);
    static inline bool emptyListIsValid() { return false; }
    static bool multiChoiceListIsValid() { return true; }
};

typedef QnCameraRecordingPolicy QnBookmarkActionPolicy;

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

template<typename CheckingPolicy>
class QnCheckResourceAndWarnDelegate: public QnResourceSelectionDialogDelegate, private CheckingPolicy
{

    using CheckingPolicy::getText;
    using CheckingPolicy::isResourceValid;

    typedef typename CheckingPolicy::resource_type ResourceType;
    typedef QnResourceSelectionDialogDelegate base_type;
public:
    QnCheckResourceAndWarnDelegate(QWidget* parent):
        QnResourceSelectionDialogDelegate(parent),
        m_warningLabel(NULL)
    {
    }
    ~QnCheckResourceAndWarnDelegate() {}

    void init(QWidget* parent) override
    {
        m_warningLabel = new QLabel(parent);
        setWarningStyle(m_warningLabel);
        parent->layout()->addWidget(m_warningLabel);
    }

    bool validate(const QSet<QnUuid>& selected) override
    {
        if (!m_warningLabel)
            return true;

        const auto message = validationMessage(selected);
        m_warningLabel->setHidden(message.isEmpty());
        m_warningLabel->setText(message);
        return true;
    }

    QString validationMessage(const QSet<QnUuid>& selected) const override
    {
        const auto resources = getResources(selected);
        const bool valid = isResourcesListValid<CheckingPolicy>(resources);
        return valid ? QString() : getText(resources);
    }

    bool isValid(const QnUuid& resourceId) const override
    {
        auto derived = getResource(resourceId).template dynamicCast<ResourceType>();

        // return true for resources of other type - so root elements will not be highlighted
        return !derived || isResourceValid(derived);
    }

    bool isMultiChoiceAllowed() const override
    {
        return CheckingPolicy::multiChoiceListIsValid();
    }

private:
    QLabel* m_warningLabel;
};

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
class QnSubjectValidationPolicy: public QnConnectionContextAware
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
class QnRequiredPermissionSubjectPolicy: public QnSubjectValidationPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnRequiredPermissionSubjectPolicy)
    using base_type = QnSubjectValidationPolicy;

public:
    explicit QnRequiredPermissionSubjectPolicy(Qn::GlobalPermission requiredPermission,
        const QString& permissionName = QString(), bool allowEmptySelection = false);

    virtual QValidator::State roleValidity(const QnUuid& roleId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;
    virtual QString calculateAlert(bool allUsers, const QSet<QnUuid>& subjects) const override;

private:
    bool isRoleValid(const QnUuid& roleId) const;

private:
    const Qn::GlobalPermission m_requiredPermission;
    const QString m_permissionName;
};
