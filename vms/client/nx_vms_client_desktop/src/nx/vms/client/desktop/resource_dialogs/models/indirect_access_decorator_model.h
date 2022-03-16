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
     * @param resourceAccessProvider Valid pointer to the resource access provider expected.
     * @param indirectAccessIconColumn Column index of the source model which will be decorated by
     *     the indirect access icon with corresponding tooltip hint where it's needed. Source model
     *     should contain at least <tt>indirectAccessIconColumn + 1</tt> columns.
     */
    IndirectAccessDecoratorModel(
        nx::core::access::ResourceAccessProvider* resourceAccessProvider,
        int indirectAccessIconColumn,
        QObject* parent = nullptr);

    /**
     * Sets resource access subject for which indirect resource access cases will be highlighted.
     * Invalid resource access subject is set by default, thus no additional data will be added to
     * the source model.
     * @param subject Resource access subject, may be invalid.
     */
    void setSubject(const QnResourceAccessSubject& subject);

    /**
     * Data may be provided by the two roles for the column specified as indirect access column:
     * - Icon by the <tt>Qt::IconRole</tt>, describing indirect access source type.
     * - Tooltip text the <Qt::TooltipRole>, rich text formatted as list containing names of
     *   accessible resources due to which resource from the corresponding row became accessible
     *   either.
     * Source model data may be also altered in the way described in the
     * <tt>setAccessAllMedia()</tt> method annotation.
     */
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    /**
     * @param value If true then every resource row provided by the source model has checkbox set
     *     and at the same time all of the items will became disabled i.e none of the indices
     *     provide <tt>Qt::ItemIsEnabled</tt> flag. This non-interactable state is used when access
     *     subject has <tt>GlobalPermission::accessAllMedia</tt> permission thus selecting specific
     *     resources has no sense.
     */
    void setAccessAllMedia(bool value);

private:
    QPointer<nx::core::access::ResourceAccessProvider> m_resourceAccessProvider;
    int m_indirectAccessIconColumn;
    QnResourceAccessSubject m_subject;
    bool m_accessAllMedia = false;
};

} // namespace nx::vms::client::desktop
