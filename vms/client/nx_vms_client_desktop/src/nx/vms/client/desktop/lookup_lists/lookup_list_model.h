// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/lookup_list_data.h>

namespace nx::vms::client::desktop {

/** Interface class for working with LookupListData from QML. */
class NX_VMS_CLIENT_DESKTOP_API LookupListModel: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::api::LookupListData data MEMBER m_data NOTIFY dataChanged)
    Q_PROPERTY(QList<QString> attributeNames
        READ attributeNames
        WRITE setAttributeNames
        NOTIFY attributeNamesChanged)

public:
    LookupListModel(QObject* parent = nullptr);
    LookupListModel(nx::vms::api::LookupListData data, QObject* parent = nullptr);
    virtual ~LookupListModel() override;

    QList<QString> attributeNames() const;
    void setAttributeNames(QList<QString> value);
    QList<int> setFilter(const QString& filterText, int resultLimit) const;

    nx::vms::api::LookupListData& rawData() { return m_data; }

signals:
    void dataChanged();
    void attributeNamesChanged();

private:
    nx::vms::api::LookupListData m_data;
};

} // namespace nx::vms::client::desktop
