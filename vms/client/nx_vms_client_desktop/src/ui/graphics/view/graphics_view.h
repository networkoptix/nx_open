// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QGraphicsView>

/**
 * Pre-configured QGraphicsView.
 */
class QnGraphicsView: public QGraphicsView
{
    Q_OBJECT

    using base_type = QGraphicsView;

public:
    QnGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr);
    virtual ~QnGraphicsView() override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void changeEvent(QEvent* event) override;
};
