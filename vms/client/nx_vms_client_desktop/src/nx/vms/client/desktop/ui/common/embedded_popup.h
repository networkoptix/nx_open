// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtQuick/QQuickItem>

#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("QtWidgets/QWidget")

namespace nx::vms::client::desktop {

/**
 * A QML object instantiating QQuickWidget, somewhat similar to Window instantiating QQuickWindow.
 * It's goal is to provide an out-of-scene QML popup capability with a top-level transparent window.
 * The QQuickWidget window is as a child of the provided viewport widget window, and positioned
 * with coordinates relative to the provided parent QML item.
 */
class EmbeddedPopup: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QWidget* viewport READ viewport WRITE setViewport NOTIFY viewportChanged)

    // Visual parent. The popup is positioned in coordinates relative to this item.
    Q_PROPERTY(QQuickItem* parent READ parentItem WRITE setParentItem NOTIFY parentItemChanged)

    Q_PROPERTY(bool visible READ isVisible WRITE setVisibleToParent NOTIFY visibleChanged)

    Q_PROPERTY(qreal margins READ margins WRITE setMargins NOTIFY marginsChanged)
    Q_PROPERTY(qreal leftMargin READ leftMargin WRITE setLeftMargin RESET resetLeftMargin
        NOTIFY leftMarginChanged)
    Q_PROPERTY(qreal topMargin READ topMargin WRITE setTopMargin RESET resetTopMargin
        NOTIFY topMarginChanged)
    Q_PROPERTY(qreal rightMargin READ rightMargin WRITE setRightMargin RESET resetRightMargin
        NOTIFY rightMarginChanged)
    Q_PROPERTY(qreal bottomMargin READ bottomMargin WRITE setBottomMargin RESET resetBottomMargin
        NOTIFY bottomMarginChanged)

    // These properties can be modified during setting.
    // Basically, you write desired values, but read effective values.
    Q_PROPERTY(qreal x READ x WRITE setX NOTIFY xChanged)
    Q_PROPERTY(qreal y READ y WRITE setY NOTIFY yChanged)
    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY heightChanged)

    Q_PROPERTY(QQuickItem* contentItem
        READ contentItem WRITE setContentItem NOTIFY contentItemChanged)

    Q_CLASSINFO("DefaultProperty", "contentItem")

public:
    explicit EmbeddedPopup(QObject* parent = nullptr);
    virtual ~EmbeddedPopup() override;

    QWidget* viewport() const;
    void setViewport(QWidget* value);

    QQuickItem* contentItem() const;
    void setContentItem(QQuickItem* value);

    QQuickItem* parentItem() const;
    void setParentItem(QQuickItem* value);

    qreal x() const;
    void setX(qreal value);

    qreal y() const;
    void setY(qreal value);

    qreal width() const;
    void setWidth(qreal value);

    qreal height() const;
    void setHeight(qreal value);

    bool isVisible() const;
    void setVisibleToParent(bool value);

    qreal margins() const;
    qreal leftMargin() const;
    qreal rightMargin() const;
    qreal topMargin() const;
    qreal bottomMargin() const;
    void setMargins(qreal value);
    void setLeftMargin(qreal value);
    void setRightMargin(qreal value);
    void setTopMargin(qreal value);
    void setBottomMargin(qreal value);
    void resetLeftMargin();
    void resetRightMargin();
    void resetTopMargin();
    void resetBottomMargin();

    Q_INVOKABLE void raise();

    static void registerQmlType();

signals:
    void viewportChanged();
    void contentItemChanged();
    void parentItemChanged();
    void xChanged();
    void yChanged();
    void widthChanged();
    void heightChanged();
    void marginsChanged();
    void leftMarginChanged();
    void topMarginChanged();
    void rightMarginChanged();
    void bottomMarginChanged();
    void visibleChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
