#pragma once

#include <QtCore/QRectF>

#include <nx/client/desktop/event_search/models/abstract_async_search_list_model.h>

namespace nx::client::desktop {

class AnalyticsSearchListModel: public AbstractAsyncSearchListModel
{
    Q_OBJECT
    using base_type = AbstractAsyncSearchListModel;

public:
    explicit AnalyticsSearchListModel(QObject* parent = nullptr);
    virtual ~AnalyticsSearchListModel() override = default;

    QRectF filterRect() const;
    void setFilterRect(const QRectF& relativeRect);

    QString filterText() const;
    void setFilterText(const QString& value);

    virtual bool isConstrained() const override;

private:
    class Private;
    Private* const d = nullptr;
};

} // namespace nx::client::desktop
