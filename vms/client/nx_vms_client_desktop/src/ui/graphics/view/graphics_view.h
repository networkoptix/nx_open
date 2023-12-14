// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsView>

#include <nx/utils/impl_ptr.h>

class QQuickWidget;

namespace nx::vms::client::desktop { class MainWindow; }

/**
 * Pre-configured QGraphicsView.
 */
class QnGraphicsView: public QGraphicsView
{
    Q_OBJECT

    using base_type = QGraphicsView;

public:
    QnGraphicsView(QGraphicsScene* scene, nx::vms::client::desktop::MainWindow* parent);
    virtual ~QnGraphicsView() override;

    QQuickWidget* quickWidget() const;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
