// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/uuid.h>
#include <nx/utils/datetime.h>

class QnWorkbenchLayout;
class QnWorkbenchItem;

class QnWorkbenchAlarmLayoutHandler: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchAlarmLayoutHandler(QObject *parent = nullptr);
    virtual ~QnWorkbenchAlarmLayoutHandler();

private:
    void openCamerasInAlarmLayout(
        const QnVirtualCameraResourceList &cameras,
        bool switchToLayoutNeeded,
        qint64 positionUs = DATETIME_NOW);

    void disableSyncForLayout(QnWorkbenchLayout* layout);
    QnVirtualCameraResourceList sortCameraResourceList(
        const QnVirtualCameraResourceList& cameraList);
    void stopShowReelIfRunning();
    void switchToLayout(QnWorkbenchLayout* layout);
    void adjustLayoutCellAspectRatio(QnWorkbenchLayout* layout);
    void addCameraOnLayout(QnWorkbenchLayout* layout, QnVirtualCameraResourcePtr camera);
    QnWorkbenchLayout* findOrCreateAlarmLayout();
    bool alarmLayoutExists() const;
    void setCameraItemPosition(QnWorkbenchLayout *layout, QnWorkbenchItem *item,
        qint64 positionUs);

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

    QnUuid m_alarmLayoutId;
};
