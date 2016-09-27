#pragma once

#include <QtCore/QObject>

#include <core/resource_access/resource_access_subject.h>

/** Public interface for all Resource Access Provider classes. */
class QnAbstractResourceAccessProvider: public QObject
{
    using base_type = QObject;
public:
    QnAbstractResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnAbstractResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const = 0;

signals:
    void accessChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource);
};
