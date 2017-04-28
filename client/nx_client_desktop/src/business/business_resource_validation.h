#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <QtWidgets/QLayout>
#include <QtWidgets/QLabel>

#include <core/resource/resource_fwd.h>
#include <core/resource/shared_resource_pointer.h>

#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/style/custom_style.h>

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

        auto resources = getResources(selected);
        bool valid = isResourcesListValid<CheckingPolicy>(resources);
        m_warningLabel->setVisible(!valid);
        if (!valid)
            m_warningLabel->setText(getText(resources));
        return true;
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

    bool validate(const QSet<QnUuid>& selected) override;
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
