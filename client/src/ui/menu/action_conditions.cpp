#include "action_conditions.h"
#include <core/resourcemanagment/resource_criterion.h>
#include <ui/graphics/items/resource_widget.h>

QnResourcePtr QnActionCondition::resource(QGraphicsItem *item) {
    if(item == NULL)
        return QnResourcePtr();

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return QnResourcePtr();

    return widget->resource();
}

QnResourceList QnActionCondition::resources(const QList<QGraphicsItem *> &items) {
    QnResourceList result;

    foreach(QGraphicsItem *item, items) {
        QnResourcePtr resource = QnActionCondition::resource(item);
        if(!resource)
            continue;

        result.push_back(resource);
    }

    return result;
}


bool QnMotionGridDisplayActionCondition::check(const QList<QGraphicsItem *> &items) {
    foreach(QGraphicsItem *item, items) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        bool isCamera = widget->resource()->flags() & QnResource::network;
        if(!isCamera)
            continue;

        if(widget->isMotionGridDisplayed()) {
            if(m_condition == HasShownGrid)
                return true;
        } else {
            if(m_condition == HasHiddenGrid)
                return true;
        }
    }

    return false;
}


QnResourceActionCondition::QnResourceActionCondition(MatchMode matchMode, QnResourceCriterion *criterion, QObject *parent): 
    QnActionCondition(parent),
    m_matchMode(matchMode),
    m_criterion(criterion)
{
    assert(criterion != NULL);
}

QnResourceActionCondition::~QnResourceActionCondition() {
    return;
}

bool QnResourceActionCondition::check(const QnResourceList &resources) {
    return checkInternal<QnResourcePtr>(resources);
}

bool QnResourceActionCondition::check(const QList<QGraphicsItem *> &items) {
    return checkInternal<QGraphicsItem *>(items);
}

template<class Item, class ItemSequence>
bool QnResourceActionCondition::checkInternal(const ItemSequence &sequence) {
    foreach(const Item &item, sequence) {
        bool matches = checkOne(item);

        if(matches && m_matchMode == OneMatches)
            return true;

        if(!matches && m_matchMode == AllMatch)
            return false;
    }

    if(m_matchMode == OneMatches) {
        return false;
    } else /* if(m_matchMode == AllMatch) */ {
        return true;
    }
}

bool QnResourceActionCondition::checkOne(const QnResourcePtr &resource) {
    return m_criterion->check(resource) == QnResourceCriterion::ACCEPT;
}

bool QnResourceActionCondition::checkOne(QGraphicsItem *item) {
    QnResourcePtr resource = this->resource(item);
    return resource ? checkOne(resource) : false;
}


