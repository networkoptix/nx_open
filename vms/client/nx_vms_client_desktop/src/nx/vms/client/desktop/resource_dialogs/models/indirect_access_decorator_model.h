// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIdentityProxyModel>
#include <QtCore/QPointer>

#include <core/resource_access/resource_access_subject.h>

namespace nx::core::access { class ResourceAccessProvider; }

namespace nx::vms::client::desktop {

/**
 * If source model provides resource by the <tt>Qn::ResourceRole</tt>, then for the given subject
 * <tt>IndirectAccessDecoratorModel</tt> will provide additional icon and tooltip describing why
 * that resource will be accessible by the subject despite resource neither accessible by global
 * permissions nor accessible by being shared directly.
 */
class IndirectAccessDecoratorModel: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:

    /**
     * @param accessProvider Valid pointer to the resource access provider expected.
     * @param indirectAccessIconColumn Column index of the source model which will be decorated by
     *     the indirect access icon and tooltip hint. Source model should contain at least
     *     <tt>indirectAccessIconColumn + 1</tt> columns.
     */
    IndirectAccessDecoratorModel(
        nx::core::access::ResourceAccessProvider* accessProvider,
        int indirectAccessIconColumn,
        QObject* parent = nullptr);

    /**
     * Sets resource access subject for which indirect resource access cases will be highlighted.
     * By default no resource access subject is set, thus no additional data added to the source
     * model.
     * @param subject Resource access subject, may be null.
     */
    void setSubject(const QnResourceAccessSubject& subject);

    /**
     * There are two types of data may be provided for the given column:
     * - Icon by the <tt>Qt::IconRole</tt>, describing indirect access source type.
     * - Tooltip rich text by the <Qt::TooltipRole> containing indirect access sources list.
     */
    virtual QVariant data(const QModelIndex& index, int role) const override;

private:
    QString getTooltipRichText(const QnResourceList& providers) const;

private:
    QPointer<nx::core::access::ResourceAccessProvider> m_resourceAccessProvider;
    int m_indirectAccessIconColumn;
    QnResourceAccessSubject m_subject;
};

} // namespace nx::vms::client::desktop
