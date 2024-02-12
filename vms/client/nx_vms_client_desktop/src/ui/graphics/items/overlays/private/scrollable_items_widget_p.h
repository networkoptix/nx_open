// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
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
    ~QnScrollableItemsWidgetPrivate();

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    QSizeF contentSizeHint(Qt::SizeHint which, const QSizeF& constraint) const;

    nx::Uuid insertItem(int index, QGraphicsWidget* item, const nx::Uuid& externalId = nx::Uuid());
    QGraphicsWidget* takeItem(const nx::Uuid& id);

    void clear();

    int count() const;

    nx::Uuid itemId(int index) const;
    QGraphicsWidget* item(int index) const;
    QGraphicsWidget* item(const nx::Uuid& id) const;

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
    QHash<nx::Uuid, QGraphicsWidget*> m_items;
    bool m_ignoreMinimumHeight = true;
};
