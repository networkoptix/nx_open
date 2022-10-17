// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QVector>

#include <core/resource/resource_fwd.h>
#include <core/resource/shared_resource_pointer.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/types/access_rights_types.h>

namespace nx::vms::client::desktop {

class AccessSubjectEditingContext;

/**
 * An information about resource access details to be used in a QML subject editing UI.
 */
struct ResourceAccessInfo
{
    Q_GADGET

public:
    enum class ProvidedVia
    {
        none,
        own,
        layout,
        videowall,
        parent,
        unknown
    };
    Q_ENUM(ProvidedVia);

    ProvidedVia providedVia{};
    QVector<QnUuid> providerGroups;
    QVector<QnResourcePtr> indirectProviders;

    bool operator==(const ResourceAccessInfo& other) const;
    bool operator!=(const ResourceAccessInfo& other) const;
};

/**
 * An utility class to obtain information about resource access details in a QML subject editing UI.
 */
class ResourceAccessInfoProvider: public QAbstractListModel
{
    Q_OBJECT

    /** A subject editing context. */
    Q_PROPERTY(nx::vms::client::desktop::AccessSubjectEditingContext* context READ context
        WRITE setContext NOTIFY contextChanged)

    /** A list of access rights of interest. */
    Q_PROPERTY(QVector<nx::vms::api::AccessRight> accessRightsList READ accessRightsList
        WRITE setAccessRightsList NOTIFY accessRightsListChanged)

    /** A resource of interest. */
    Q_PROPERTY(QnResource* resource READ resource WRITE setResource NOTIFY resourceChanged)

    /** Access details per each access right from `accessRightList`. */
    Q_PROPERTY(QVector<nx::vms::client::desktop::ResourceAccessInfo> info
        READ info NOTIFY infoChanged)

    using base_type = QAbstractListModel;

public:
    enum Roles
    {
        providedVia = Qt::UserRole + 1,
        isOwn,
    };

public:
    explicit ResourceAccessInfoProvider(QObject* parent = nullptr);
    virtual ~ResourceAccessInfoProvider() override;

    AccessSubjectEditingContext* context() const;
    void setContext(AccessSubjectEditingContext* value);

    QVector<nx::vms::api::AccessRight> accessRightsList() const;
    void setAccessRightsList(const QVector<nx::vms::api::AccessRight>& value);

    QnResource* resource() const;
    void setResource(QnResource* value);

    QVector<ResourceAccessInfo> info() const;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    QHash<int, QByteArray> roleNames() const override;

    /** @returns ProvidedVia::layout, ProvidedVia::videowall or ProvidedVia::unknown. */
    static Q_INVOKABLE ResourceAccessInfo::ProvidedVia providerType(QnResource* provider);

    Q_INVOKABLE void toggleAll();

    static void registerQmlTypes();

private:
    QString accessDetailsText(
        const QnUuid& resourceId,
        nx::vms::api::AccessRight accessRight) const;

    QString accessDetailsText(const ResourceAccessInfo& accessInfo) const;

signals:
    void contextChanged();
    void accessRightsListChanged();
    void resourceChanged();
    void infoChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
