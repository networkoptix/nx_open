// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/vms/rules/action_builder_fields/text_with_fields.h>

namespace nx::vms::client::desktop::rules {

class EventParametersModel: public QAbstractListModel
{
    Q_OBJECT
public:
    EventParametersModel(
        const nx::vms::rules::TextWithFields::EventParametersByGroup& eventParameters,
        QObject* parent);

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

private:
    QList<QString> groupedEventParametersNames;
    bool isSeparator(const QModelIndex& index) const;
};

} // namespace nx::vms::client::desktop::rules
