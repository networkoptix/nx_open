#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QStyledItemDelegate>

#include <client/client_color_types.h>
#include <ui/customization/customized.h>

class QnResourceItemDelegate;

class QnRecordingStatsItemDelegate : public Customized<QStyledItemDelegate>
{
    Q_OBJECT
    Q_PROPERTY(QnRecordingStatsColors colors READ colors WRITE setColors)

    using base_type = Customized<QStyledItemDelegate>;

public:
    explicit QnRecordingStatsItemDelegate(QObject* parent = nullptr);
    virtual ~QnRecordingStatsItemDelegate();

    const QnRecordingStatsColors& colors() const;
    void setColors(const QnRecordingStatsColors& colors);

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

private:
    enum class RowType
    {
        normal,
        foreign,
        total
    };

    RowType rowType(const QModelIndex& index) const;

private:
    QScopedPointer<QnResourceItemDelegate> m_resourceDelegate;
    QnRecordingStatsColors m_colors;
};
