#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

class QGraphicsWidget;
class QGraphicsLinearLayout;
class QnGraphicsScrollArea;
class QnScrollableItemsWidget;

class QnScrollableItemsWidgetPrivate: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_DECLARE_PUBLIC(QnScrollableItemsWidget);
    QnScrollableItemsWidget* const q_ptr;

public:
    QnScrollableItemsWidgetPrivate(QnScrollableItemsWidget* parent);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    QSizeF contentSizeHint(Qt::SizeHint which, const QSizeF& constraint) const;

    QnUuid insertItem(int index, QGraphicsWidget* item, const QnUuid& externalId = QnUuid());
    QGraphicsWidget* takeItem(const QnUuid& id);

    void clear();

    int count() const;

    QGraphicsWidget* item(int index) const;
    QGraphicsWidget* item(const QnUuid& id) const;

    qreal spacing() const;
    void setSpacing(qreal value);

    int lineHeight() const;
    void setLineHeight(int value);

private:
    void updateContentPosition();

private:
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignTop;
    QnGraphicsScrollArea* const m_scrollArea;
    QGraphicsLinearLayout* const m_contentLayout;
    QGraphicsLinearLayout* m_mainLayout{nullptr};
    QHash<QnUuid, QGraphicsWidget*> m_items;
};
