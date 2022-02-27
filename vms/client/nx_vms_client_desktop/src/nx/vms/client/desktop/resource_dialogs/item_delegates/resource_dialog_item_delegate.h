// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {

class ResourceDialogItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    ResourceDialogItemDelegate(QObject* parent = nullptr);
    virtual ~ResourceDialogItemDelegate() override;

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

    bool showRecordingIndicator() const;
    void setShowRecordingIndicator(bool show);

    static bool isSpacer(const QModelIndex& index);
    static bool isSeparator(const QModelIndex& index);

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    virtual int decoratorWidth(const QModelIndex& index) const;

    void paintItemText(
        QPainter* painter,
        QStyleOptionViewItem& option,
        const QModelIndex& index,
        const QColor& mainColor,
        const QColor& extraColor,
        const QColor& invalidColor) const;

    void paintItemIcon(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index,
        QIcon::Mode mode) const;

    void paintRecordingIndicator(QPainter* painter,
        const QRect& iconRect,
        const QModelIndex& index) const;

    void paintSeparator(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

private:
    struct Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
