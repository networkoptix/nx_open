// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_search_model_adapter.h"

#include <QtQml/QtQml>

#include "abstract_async_search_list_model.h"
#include "visible_item_data_decorator_model.h"

namespace nx::vms::client::core {

void EventSearchModelAdapter::registerQmlType()
{
    qmlRegisterUncreatableType<EventSearchModelAdapter>("nx.vms.client.core", 1, 0,
        "EventSearchModel", "Cannot create instance of EventSearchModel");
}

bool EventSearchModelAdapter::fetchInProgress() const
{
    if (const auto model = searchModel())
        return model->fetchInProgress();

    return false;
}

bool EventSearchModelAdapter::fetchData(const FetchRequest& request)
{
    if (const auto model = searchModel())
        return model->fetchData(request);

    return false;
}

void EventSearchModelAdapter::clear()
{
    if (const auto model = searchModel())
        model->clear();
}

FetchRequest EventSearchModelAdapter::requestForDirection(
    EventSearch::FetchDirection direction) const
{
    if (const auto model = searchModel())
        return model->requestForDirection(direction);

    return {};
}

void EventSearchModelAdapter::setSearchModel(AbstractAsyncSearchListModel* value)
{
    m_modelConnections.reset();

    if (!value)
    {
        setSourceModel(nullptr);
        return;
    }

    m_modelConnections <<
        connect(value, &AbstractAsyncSearchListModel::asyncFetchStarted,
           this, &EventSearchModelAdapter::asyncFetchStarted);

    m_modelConnections <<
        connect(value, &AbstractAsyncSearchListModel::fetchFinished,
            this, &EventSearchModelAdapter::fetchFinished);

    const auto visibleItemDecorator = new VisibleItemDataDecoratorModel(/*settings*/{}, this);
    visibleItemDecorator->setSourceModel(value);
    setSourceModel(visibleItemDecorator);
}

AbstractAsyncSearchListModel* EventSearchModelAdapter::searchModel() const
{
    const auto visibleItemDecorator = qobject_cast<VisibleItemDataDecoratorModel*>(sourceModel());
    return visibleItemDecorator
        ? qobject_cast<AbstractAsyncSearchListModel*>(visibleItemDecorator->sourceModel())
        : nullptr;
}

} // namespace nx::vms::client::core
