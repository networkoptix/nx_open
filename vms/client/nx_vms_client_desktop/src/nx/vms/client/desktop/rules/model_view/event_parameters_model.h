// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop::rules {

class NX_VMS_CLIENT_DESKTOP_API EventParametersModel: public QAbstractListModel
{
    Q_OBJECT
public:
    EventParametersModel(const QList<QString>& eventParameters, QObject* parent = nullptr);
    ~EventParametersModel();

    int rowCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    void expandGroup(const QString& groupName);
    void resetExpandedGroup();
    bool isSubGroupName(const QString& groupName);
    bool isCorrectParameter(const QString& eventParameter);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::rules
