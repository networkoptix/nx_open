// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtQuick/QQuickItem>

#include <nx/utils/impl_ptr.h>

class QQuickText;

namespace nx::vms::client::core {

class ValuesText: public QQuickItem
{
    Q_OBJECT
    using base_type = QQuickItem;

    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged)
    Q_PROPERTY(QStringList values READ values WRITE setValues NOTIFY valuesChanged)
    Q_PROPERTY(
        QStringList colorValues READ colorValues WRITE setColorValues NOTIFY colorValuesChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(
        QColor appendixColor READ appendixColor WRITE setAppendixColor NOTIFY appendixColorChanged)
    Q_PROPERTY(QString separator READ separator WRITE setSeparator NOTIFY separatorChanged);
    Q_PROPERTY(
        int maximumLineCount
        READ maximumLineCount
        WRITE setMaximumLineCount
        NOTIFY maximumLineCountChanged)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment NOTIFY alignmentChanged)
    Q_PROPERTY(qreal effectiveWidth READ effectiveWidth NOTIFY effectiveWidthChanged)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing() NOTIFY spacingChanged)
    Q_PROPERTY(
        qreal colorBoxSize
        READ colorBoxSize
        WRITE setColorBoxSize
        NOTIFY colorBoxSizeChanged)

public:
    ValuesText(QQuickItem* parent = nullptr);
    virtual ~ValuesText() override;

    QStringList values() const;
    void setValues(const QStringList& values);

    QStringList colorValues() const;
    void setColorValues(const QStringList& values);

    QFont font() const;
    void setFont(const QFont& value);

    QColor color() const;
    void setColor(const QColor& value);

    QColor appendixColor() const;
    void setAppendixColor(const QColor& value);

    QString separator() const;
    void setSeparator(const QString& value);

    int maximumLineCount() const;
    void setMaximumLineCount(int value);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment value);

    qreal effectiveWidth();

    int spacing() const;
    void setSpacing(int value);

    qreal colorBoxSize() const;
    void setColorBoxSize(qreal value);

    static void registerQmlType();

protected:
    virtual void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    virtual QSGNode* updatePaintNode(
        QSGNode* oldNode, QQuickItem::UpdatePaintNodeData* data) override;

signals:
    void valuesChanged();
    void colorValuesChanged();
    void fontChanged();
    void colorChanged();
    void appendixColorChanged();
    void separatorChanged();
    void maximumLineCountChanged();
    void alignmentChanged();
    void effectiveWidthChanged();
    void spacingChanged();
    void colorBoxSizeChanged();

private:
    void updateItemView();
    void updateTextRow();
    void updateColorRow();
    void setEffectiveWidth(qreal value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
