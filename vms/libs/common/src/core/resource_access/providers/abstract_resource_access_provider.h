#pragma once

#include <QtCore/QObject>

#include <nx/core/access/access_types.h>

#include <utils/common/connective.h>
#include <utils/common/updatable.h>
#include <core/resource/resource_fwd.h>

class QnResourceAccessSubject;

/** Public interface for all Resource Access Provider classes. */
class QnAbstractResourceAccessProvider:
    public Connective<QObject>,
    public QnUpdatable
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    using Mode = nx::core::access::Mode;
    using Source = nx::core::access::Source;

    QnAbstractResourceAccessProvider(Mode mode, QObject* parent = nullptr);
    virtual ~QnAbstractResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const = 0;

    virtual QSet<QnUuid> accessibleResources(const QnResourceAccessSubject& subject) const = 0;

    /**
    * Check the way how the resource is accessible to the subject.
    *
    * @param subject User or role
    * @param resource Target resource
    * @param providers List of resources, that grant indirect access. In case of direct access it
    *     will be empty. Actual if the resource is accessible through shared layouts or videowalls.
    * @return Access source type.
    */
    virtual Source accessibleVia(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList* providers = nullptr) const = 0;

    Mode mode() const;

signals:
    void accessChanged(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource, Source value);

private:
    const Mode m_mode;
};
