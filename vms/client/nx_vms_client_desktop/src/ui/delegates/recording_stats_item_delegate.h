// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QStyledItemDelegate>

class QnResourceItemDelegate;

class QnRecordingStatsItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    explicit QnRecordingStatsItemDelegate(QObject* parent = nullptr);
    virtual ~QnRecordingStatsItemDelegate();

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

private:
    QScopedPointer<QnResourceItemDelegate> m_resourceDelegate;
};
