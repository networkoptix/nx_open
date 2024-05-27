// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>

namespace nx::vms::client::desktop::rules {

class NX_VMS_CLIENT_DESKTOP_API EventParametersModel: public QAbstractListModel
{
    Q_OBJECT
public:
    EventParametersModel(
        const vms::rules::utils::EventParameterHelper::EventParametersNames& eventParameters,
        QObject* parent = nullptr);
    ~EventParametersModel();

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    void expandGroup(const QString& groupName);
    void resetExpandedGroup();
    bool isSubGroupName(const QString& groupName);
    bool isCorrectParameter(const QString& eventParameter);
    void filter(const QString& filter);
    void setDefaultVisibleElements();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::rules
