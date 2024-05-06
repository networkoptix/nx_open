// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QRect>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

Q_MOC_INCLUDE("core/resource/layout_resource.h")

namespace nx::vms::client::desktop {

class LayoutModel: public QAbstractListModel
{
    using base_type = QAbstractListModel;

    Q_OBJECT
    Q_PROPERTY(QnLayoutResource* layout READ rawLayout WRITE setRawLayout NOTIFY layoutChanged)
    Q_PROPERTY(QRect gridBoundingRect READ gridBoundingRect NOTIFY gridBoundingRectChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY lockedChanged)
    Q_PROPERTY(bool isShowreelReview READ isShowreelReview NOTIFY layoutChanged)

public:
    enum class Roles
    {
        itemId = Qt::UserRole + 1,
        itemData,
    };

    explicit LayoutModel(QObject* parent = nullptr);
    virtual ~LayoutModel() override;

    QnLayoutResource* rawLayout() const;
    void setRawLayout(QnLayoutResource* value);

    QRect gridBoundingRect() const;

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    bool locked() const;
    bool isShowreelReview() const;

    static void registerQmlType();

signals:
    void layoutChanged();
    void lockedChanged();
    void gridBoundingRectChanged();

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace nx::vms::client::desktop
