// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/connection_types.h>

class QnLongRunableCleanup;
class QnStoragePluginFactory;

namespace nx::metrics { struct ApplicationMetricsStorage; }
namespace nx::i18n { class TranslationManager; }

namespace nx::vms::common {

/**
 * Main context of the VMS applications. Exists through all application lifetime and is accessible
 * from anywhere using `instance()` method.
 *
 * Works as a storage for the most basic and generic singletons, needed in all common projects.
 * Initializes networking and ffmpeg library, stores pool for all long-runnable threads, keeps the
 * single synchronized time source.
 */
class NX_VMS_COMMON_API ApplicationContext: public QObject
{
protected:
    using PeerType = nx::vms::api::PeerType;

public:
    ApplicationContext(
        PeerType localPeerType = PeerType::notDefined,
        const QString& customCloudHost = QString(),
        QObject* parent = nullptr);
    virtual ~ApplicationContext() override;

    /**
     * Main context of the VMS applications. Exists through all application lifetime.
     */
    static ApplicationContext* instance();

    void initNetworking(const QString& customCloudHost = QString());
    void deinitNetworking();

    PeerType localPeerType() const;

    /** Application language locale code (in form `en_US`). */
    QString locale() const;

    /** Set application language locale code (in form `en_US`). */
    void setLocale(const QString& value);

    /** Application-wide translation manager. Should always exist. */
    nx::i18n::TranslationManager* translationManager() const;

    /** Set application-wide translation manager. Weak pointer is stored. */
    void setTranslationManager(nx::i18n::TranslationManager* value);

    void setModuleShortId(const nx::Uuid& id, int number);
    int moduleShortId(const nx::Uuid& id) const;
    QString moduleDisplayName(const nx::Uuid& id) const;

    QnStoragePluginFactory* storagePluginFactory() const;
    QnLongRunableCleanup* longRunableCleanup() const;
    nx::metrics::ApplicationMetricsStorage* metrics() const;

    template<typename ContextType>
    ContextType* as()
    {
        return qobject_cast<ContextType*>(this);
    }

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

inline ApplicationContext* appContext() { return ApplicationContext::instance(); }

} // namespace nx::vms::common
