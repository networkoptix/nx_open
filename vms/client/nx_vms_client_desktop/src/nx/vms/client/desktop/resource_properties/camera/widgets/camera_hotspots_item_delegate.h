// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtGui/QColor>
#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {

class ItemViewHoverTracker;

class CameraHotspotsItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    CameraHotspotsItemDelegate(QObject* parent = nullptr);
    virtual ~CameraHotspotsItemDelegate() override;

    void setItemViewHoverTracker(ItemViewHoverTracker* hoverTracker);

    /**
     * @return List of predefined colors available in the hotspots color palette.
     */
    static QList<QColor> hotspotsPalette();

    /**
     * @return Index of one of the predefined colors from the hotspots color palette, if it has
     *     one, std::nullopt otherwise.
     */
    static std::optional<int> paletteColorIndex(const QColor& color);

    /**
     * @param pos The position of the cursor.
     * @param viewItemRect Palette item rect, in the same coordinates system as cursor position.
     * @return Index of one of the predefined colors from the hotspots color palette, if the given
     *     point falls within the selection region of that color, std::nullopt otherwise.
     */
    static std::optional<int> paletteColorIndexAtPos(const QPoint& pos, const QRect& viewItemRect);

    QRect itemContentsRect(const QModelIndex& index, QAbstractItemView* itemView) const;

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

private:
    void paintColorPicker(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    ItemViewHoverTracker* m_hoverTracker = nullptr;
};

} // namespace nx::vms::client::desktop
