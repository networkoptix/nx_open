#pragma once

#include <QtCore/QObject>

#include <core/resource/videowall_item.h>
#include <core/resource/videowall_control_message.h>
#include <core/resource/videowall_pc_data.h>
#include <utils/common/connective.h>
#include <nx_ec/data/api_fwd.h>

class QnMobileClientUiController;

class QnVideowallHandler: public Connective<QObject>
{
    Q_OBJECT

    using base_type = Connective<QObject>;

public:
    explicit QnVideowallHandler(QObject* parent = nullptr);
    virtual ~QnVideowallHandler() = default;

    void setUiController(QnMobileClientUiController* uiController);

private:
    void handleMessage(
        const QnVideoWallControlMessage& message,
        const QnUuid &controllerUuid = QnUuid(),
        qint64 sequence = -1);

    void at_controlMessageReceived(
        const ec2::ApiVideowallControlMessageData& message);

private:
    QPointer<QnMobileClientUiController> m_uiController;
};
