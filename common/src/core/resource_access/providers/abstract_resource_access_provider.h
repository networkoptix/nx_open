#pragma once

#include <QtCore/QObject>

#include <core/resource_access/resource_access_subject.h>

#include <utils/common/connective.h>
#include <utils/common/updatable.h>

/** Public interface for all Resource Access Provider classes. */
class QnAbstractResourceAccessProvider:
    public Connective<QObject>,
    public QnUpdatable
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    /** Types of base providers, sorted by priority. */
    enum class Source
    {
        none,
        permissions,    //< Accessible by permissions.
        shared,         //< Accessible by direct sharing.
        layout,         //< Accessible by placing on shared layout.
        videowall,      //< Accessible by placing on videowall.
        tour,           //< Accessible via layout tour
    };

    QnAbstractResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnAbstractResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const = 0;

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

signals:
    void accessChanged(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource, Source value);
};

inline uint qHash(QnAbstractResourceAccessProvider::Source value, uint seed)
{
    return qHash(static_cast<int>(value), seed);
}