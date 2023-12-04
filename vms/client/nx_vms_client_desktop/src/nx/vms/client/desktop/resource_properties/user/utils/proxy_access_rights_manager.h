// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource_access/abstract_access_rights_manager.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

/**
 * An utility class for one subject access rights editing.
 * This class is not thread safe.
 */
class ProxyAccessRightsManager: public nx::core::access::AbstractAccessRightsManager
{
    Q_OBJECT
    using base_type = nx::core::access::AbstractAccessRightsManager;

public:
    explicit ProxyAccessRightsManager(
        nx::core::access::AbstractAccessRightsManager* source,
        QObject* parent = nullptr);

    virtual ~ProxyAccessRightsManager() override;

    /** A current subject being edited. */
    QnUuid currentSubjectId() const;
    void setCurrentSubjectId(const QnUuid& value);

    virtual nx::core::access::ResourceAccessMap ownResourceAccessMap(
        const QnUuid & subjectId) const override;

    /** Overrides current subject access rights map. */
    void setOwnResourceAccessMap(const nx::core::access::ResourceAccessMap& value);

    /** Reverts current subject access rights editing. */
    void resetOwnResourceAccessMap();

    /** Returns whether current subject access rights were changed (overridden). */
    bool hasChanges() const;

signals:
    void proxyAccessRightsAboutToBeChanged();
    void proxyAccessRightsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
