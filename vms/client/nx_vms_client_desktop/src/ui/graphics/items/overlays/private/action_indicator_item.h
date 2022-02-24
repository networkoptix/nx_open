// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtWidgets/QGraphicsWidget>

#include <nx/utils/impl_ptr.h>

class QnActionIndicatorItem: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    explicit QnActionIndicatorItem(QGraphicsWidget* parent = nullptr);
    virtual ~QnActionIndicatorItem() override;

    QString text() const;
    void setText(const QString& value);

    void clear(std::chrono::milliseconds after);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget = nullptr) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
