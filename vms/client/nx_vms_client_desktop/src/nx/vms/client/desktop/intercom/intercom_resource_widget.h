// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>

#include <api/model/api_ioport_data.h>
#include <nx/utils/uuid.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

namespace nx::vms::client::desktop {

class IntercomResourceWidget: public QnMediaResourceWidget
{
    Q_OBJECT

    using base_type = QnMediaResourceWidget;

public:
    IntercomResourceWidget(
        QnWorkbenchContext* context,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);

    virtual ~IntercomResourceWidget() override;

protected:
    virtual int calculateButtonsVisibility() const override;

private:
    QString getOutputId(nx::vms::api::ExtendedCameraOutput outputType) const;

    void setOutputPortState(
        nx::vms::api::ExtendedCameraOutput outputType,
        nx::vms::api::EventState newState) const;

private:
    QMap<QString, QString> m_outputNameToId;
};

} // namespace nx::vms::client::desktop
