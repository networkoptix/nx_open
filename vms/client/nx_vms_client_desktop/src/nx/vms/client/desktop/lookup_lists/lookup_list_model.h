// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

/** Interface class for working with LookupListData from QML. */
class NX_VMS_CLIENT_DESKTOP_API LookupListModel: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::api::LookupListData data MEMBER m_data NOTIFY dataChanged)
    Q_PROPERTY(QString objectTypeId READ objectTypeId WRITE setObjectTypeId
        NOTIFY objectTypeIdChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QList<QString> attributeNames
        READ attributeNames
        WRITE setAttributeNames
        NOTIFY attributeNamesChanged)
    Q_PROPERTY(bool isGeneric READ isGeneric NOTIFY isGenericChanged)

public:
    LookupListModel(QObject* parent = nullptr);
    LookupListModel(nx::vms::api::LookupListData data, QObject* parent = nullptr);
    virtual ~LookupListModel() override;

    QList<QString> attributeNames() const;
    void setAttributeNames(QList<QString> value);
    nx::vms::api::LookupListData& rawData() { return m_data; }
    bool isGeneric() const;
    QString objectTypeId() const;
    void setObjectTypeId(const QString& objectTypeId);
    QString name() const;
    void setName(const QString& name);

    Q_INVOKABLE int countOfAssociatedVmsRules(SystemContext* systemContext) const;

signals:
    void dataChanged();
    void attributeNamesChanged();
    void isGenericChanged();
    void objectTypeIdChanged();
    void nameChanged();

private:
    nx::vms::api::LookupListData m_data;
};

} // namespace nx::vms::client::desktop
