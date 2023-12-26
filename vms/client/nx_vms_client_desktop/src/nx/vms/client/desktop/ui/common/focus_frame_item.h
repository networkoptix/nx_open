// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>
#include <QtQuick/QQuickItem>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class FocusFrameItem: public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(int frameWidth READ frameWidth WRITE setFrameWidth NOTIFY frameWidthChanged)

    using base_type = QQuickItem;

public:
    FocusFrameItem(QQuickItem* parent = nullptr);
    virtual ~FocusFrameItem() override;

    QColor color() const;
    void setColor(const QColor& color);

    int frameWidth() const;
    void setFrameWidth(int frameWidth);

    static void registerQmlType();

signals:
    void colorChanged();
    void frameWidthChanged();

protected:
    virtual QSGNode* updatePaintNode(
        QSGNode* node,
        UpdatePaintNodeData* updatePaintNodeData) override;
    virtual void releaseResources() override;

private slots:
    void handleWindowChanged(QQuickWindow* win);
    void cleanup();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
