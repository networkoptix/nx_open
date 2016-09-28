#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

/** Handles access to cameras and web pages, placed on shared layouts. */
class QnSharedLayoutAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;
public:
    QnSharedLayoutAccessProvider(QObject* parent = nullptr);
    virtual ~QnSharedLayoutAccessProvider();

protected:
    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;
};