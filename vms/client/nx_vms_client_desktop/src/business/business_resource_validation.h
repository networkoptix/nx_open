// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS
#include <QtWidgets/QLabel>

#include <core/resource/resource_fwd.h>
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
    QnSendEmailActionDelegate(
        nx::vms::client::desktop::SystemContext* systemContext,
        QWidget* parent);
    ~QnSendEmailActionDelegate() {}

    void init(QWidget* parent) override;

    QString validationMessage(const QSet<nx::Uuid>& selected) const override;
    bool isValid(const nx::Uuid& resourceId) const override;

    static bool isValidList(const QSet<nx::Uuid>& ids, const QString& additional);

    static QString getText(const QSet<nx::Uuid>& ids, const bool detailed,
        const QString& additional);
private:
    static QStringList parseAdditional(const QString& additional);
    static bool isValidUser(const QnUserResourcePtr& user);
private:
    QLabel* m_warningLabel;
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
