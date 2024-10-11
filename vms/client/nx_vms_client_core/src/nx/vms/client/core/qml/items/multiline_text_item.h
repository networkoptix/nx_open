// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QTextOption>
#include <QtQuick/QQuickItem>

#include <nx/utils/impl_ptr.h>

class QSGNode;

namespace nx::vms::client::core {

/**
 * Currently no QtQuick text component allows to elide HTML text that doesn't fit by height.
 * This component is designed to provide such functionality.
 * Current implementation is not interactive (hyperlinks are not hoverable nor clickable).
 */
class MultilineTextItem: public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(WrapMode wrapMode READ wrapMode WRITE setWrapMode NOTIFY wrapModeChanged)
    Q_PROPERTY(QString elideMarker READ elideMarker WRITE setElideMarker NOTIFY elideMarkerChanged)

    using base_type = QQuickItem;

public:
    enum WrapMode
    {
        NoWrap = QTextOption::NoWrap,
        WordWrap = QTextOption::WordWrap,
        WrapAnywhere = QTextOption::WrapAnywhere,
        Wrap = QTextOption::WrapAtWordBoundaryOrAnywhere
    };
    Q_ENUM(WrapMode)

public:
    explicit MultilineTextItem(QQuickItem* parent = nullptr);
    virtual ~MultilineTextItem() override;

    QString text() const;
    void setText(const QString& value);

    QColor color() const;
    void setColor(const QColor& value);

    QFont font() const;
    void setFont(const QFont& value);

    WrapMode wrapMode() const;
    void setWrapMode(WrapMode value);

    /** A replacement for a text line if the next line doesn't fit and is elided. */
    QString elideMarker() const;
    void setElideMarker(const QString& value);

    static void registerQmlType();

protected:
    virtual void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    virtual QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

signals:
    void textChanged();
    void colorChanged();
    void fontChanged();
    void wrapModeChanged();
    void elideMarkerChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
