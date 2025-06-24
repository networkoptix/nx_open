// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

namespace nx::vms::client::core {

class BookmarkSearchListModel;

class NX_VMS_CLIENT_CORE_API BookmarkSearchSetup: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(bool searchSharedOnly
        READ searchSharedOnly
        WRITE setSearchSharedOnly
        NOTIFY parametersChanged)

public:
    static void registerQmlType();

    explicit BookmarkSearchSetup(
        BookmarkSearchListModel* model,
        QObject* parent = nullptr);
    virtual ~BookmarkSearchSetup() override;

    bool searchSharedOnly() const;
    void setSearchSharedOnly(const bool value);

signals:
    void parametersChanged();

private:
    QPointer<BookmarkSearchListModel> m_model;
};

} // namespace nx::vms::client::core
