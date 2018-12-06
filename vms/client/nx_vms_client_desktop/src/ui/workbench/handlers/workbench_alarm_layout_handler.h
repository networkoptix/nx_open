#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>
#include <nx/utils/uuid.h>

class QnWorkbenchLayout;
class QnWorkbenchItem;

class QnWorkbenchAlarmLayoutHandler: public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnWorkbenchAlarmLayoutHandler(QObject *parent = nullptr);
    virtual ~QnWorkbenchAlarmLayoutHandler();

private:
    void openCamerasInAlarmLayout(const QnVirtualCameraResourceList &cameras, bool switchToLayout);

    QnWorkbenchLayout* findOrCreateAlarmLayout();
    bool alarmLayoutExists() const;

    void jumpToLive(QnWorkbenchLayout *layout, QnWorkbenchItem *item);

    /**
     *  Check if current client instance is the main one.
     *  Main instance is the first that was started on the given PC.
     */
    bool currentInstanceIsMain() const;

private:
    struct ActionKey
    {
        QnUuid ruleId;
        qint64 timestampUs;

        ActionKey() : ruleId(), timestampUs(0) {}
        ActionKey(QnUuid ruleId, qint64 timestampUs) : ruleId(ruleId), timestampUs(timestampUs) {}
        bool operator==(const ActionKey &right) const { return ruleId == right.ruleId && timestampUs == right.timestampUs; }
    };

    /**
     * Actions that are currently handled.
     * Current business actions implementation will send us 'Alarm' action once for every camera that
     * is enumerated in action resources. But on the client side we are handling all cameras at once
     * to make sure they will be displayed on layout in the same order. So we are keeping list of all
     * recently handled actions to avoid duplicated method calls. List is cleaned by timer.
     */
    QList<ActionKey> m_processingActions;

    QnLayoutResourcePtr m_alarmLayout;
};
