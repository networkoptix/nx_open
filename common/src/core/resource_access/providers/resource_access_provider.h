#pragma once

#include <core/resource_access/providers/abstract_resource_access_provider.h>

#include <nx/utils/singleton.h>

/**
 * Class-facade that collects data from base providers.
 * Governs ways of access to resources. For example, camera may be accessible
 * directly - or by shared layout where it is located.
 */
class QnResourceAccessProvider: public QnAbstractResourceAccessProvider,
    public Singleton<QnResourceAccessProvider>
{
    using base_type = QnAbstractResourceAccessProvider;
public:
    QnResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    virtual Source accessibleVia(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    /**
     * Add new base provider to the end of the list.
     * NOTICE: this class will take ownership!
     */
    void addBaseProvider(QnAbstractResourceAccessProvider* provider);

    /**
    * Add new base provider to the specified place.
    * NOTICE: this class will take ownership!
    */
    void insertBaseProvider(int index, QnAbstractResourceAccessProvider* provider);

    /**
    * Remove base provider from the module.
    * NOTICE: this class will release ownership! You must remove the provider yourself.
    */
    void removeBaseProvider(QnAbstractResourceAccessProvider* provider);

    QList<QnAbstractResourceAccessProvider*> providers() const;

private:
    void handleBaseProviderAccessChanged(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource, Source value);

private:
    QList<QnAbstractResourceAccessProvider*> m_providers;
};

#define qnResourceAccessProvider QnResourceAccessProvider::instance()
