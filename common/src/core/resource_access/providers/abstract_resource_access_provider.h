#pragma once

#include <QtCore/QObject>

#include <core/resource_access/resource_access_subject.h>

#include <utils/common/connective.h>

/** Public interface for all Resource Access Provider classes. */
class QnAbstractResourceAccessProvider: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    /** Types of base providers, sorted by priority. */
    enum class Source
    {
        none,
        direct,     /* Accessible by permissions. */
        shared,     /* Accessible by direct sharing. */
        layout,     /* Accessible by placing on shared layout. */
        videowall   /* Accessible by placing on videowall. */
    };

    QnAbstractResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnAbstractResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const = 0;

    virtual Source accessibleVia(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const = 0;

signals:
    void accessChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        Source value);
};
