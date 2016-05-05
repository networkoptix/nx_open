#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QStyledItemDelegate>

#include <client/client_color_types.h>

class QnWorkbench;

class QnResourceItemDelegate : public QStyledItemDelegate
{
    typedef QStyledItemDelegate base_type;

    Q_PROPERTY(QnResourceItemColors colors READ colors WRITE setColors)

public:
    explicit QnResourceItemDelegate(QObject* parent = nullptr);

    QnWorkbench* workbench() const;
    void setWorkbench(QnWorkbench* workbench);

    const QnResourceItemColors& colors() const;
    void setColors(const QnResourceItemColors& colors);

protected:
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& styleOption, const QModelIndex& index) const;
    virtual QSize sizeHint(const QStyleOptionViewItem& styleOption, const QModelIndex& index) const override;

    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    virtual void destroyEditor(QWidget* editor, const QModelIndex& index) const override;

    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    enum class ItemState
    {
        Normal,
        Selected,
        Accented
    };

    ItemState itemState(const QModelIndex& index) const;

private:
    QPointer<QnWorkbench> m_workbench;
    QIcon m_recordingIcon, m_scheduledIcon, m_buggyIcon;
    QnResourceItemColors m_colors;
};
