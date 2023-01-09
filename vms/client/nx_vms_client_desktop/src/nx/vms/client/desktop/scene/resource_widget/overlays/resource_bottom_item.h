// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsWidget>

class GraphicsLabel;
class QnImageButtonWidget;
class QnHtmlTextItem;

namespace nx::vms::client::desktop {

class ResourceBottomItem: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    ResourceBottomItem(QGraphicsItem* parent = nullptr);
    virtual ~ResourceBottomItem() override;

    QnHtmlTextItem* positionAndRecordingStatus() const;
    QnHtmlTextItem* pauseButton() const;
    void setVisibleButtons(int buttons);
    int visibleButtons();

private:
    QnHtmlTextItem* m_pauseButton = nullptr;
    QnHtmlTextItem* m_positionAndRecording = nullptr;
};

}; // namespace nx::vms::client::desktop
