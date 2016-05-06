#pragma once

#include <QtWidgets/QStyledItemDelegate>
#include <ui/workbench/workbench_context_aware.h>

struct QnAuditRecord;

class QnAuditItemDelegate : public QStyledItemDelegate, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QStyledItemDelegate base_type;

public:
    explicit QnAuditItemDelegate(QObject* parent = nullptr);

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& styleOption, const QModelIndex& index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    virtual bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

signals:
    void buttonClicked(const QModelIndex& index);
    void descriptionPressed(const QModelIndex& index);

private:
    void paintDateTime(QPainter* painter, const QStyleOptionViewItem& option, int dateTimeSecs) const;
    void paintDescription(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index, const QnAuditRecord* record) const;
    void paintUserActivity(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

    int textWidth(const QFont& font, const QString& textData, bool isBold = false) const;

    QSize defaultSizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;
    QSize descriptionSizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const;

private:
    QModelIndex m_lastHoveredButtonIndex;   /* Stores model index of item button that was hovered last */
    QModelIndex m_lastPressedButtonIndex;   /* Stores model index of item button that was pressed      */
    QRect m_lastHoveredButtonRect;          /* Stores rectangle of item button that was hovered last   */
    bool m_buttonCapturedMouse;             /* Indicates that left button was pressed on item button   */

    mutable QHash<QString, int> m_sizeHintHash;
    mutable QHash<QString, int> m_boldSizeHintHash;
};

