// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/mobile/session/session_manager.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::mobile {

class SessionManager;

class AudioController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QnResource* resource
        READ rawResource
        WRITE setRawResource
        NOTIFY resourceChanged)

    Q_PROPERTY(bool audioEnabled
        READ audioEnabled
        WRITE setAudioEnabled
        NOTIFY audioEnabledChanged)

    Q_PROPERTY(nx::vms::client::mobile::SessionManager* serverSessionManager
        READ serverSessionManager
        WRITE setServerSessionManager
        NOTIFY serverSessionManagerChanged)

public:
    static void registerQmlType();

    AudioController(QObject* parent = nullptr);
    virtual ~AudioController();

    void setServerSessionManager(SessionManager* value);
    SessionManager* serverSessionManager() const;

    void setResource(const QnResourcePtr& value);
    QnResourcePtr resource();

    void setAudioEnabled(bool value);
    bool audioEnabled() const;

signals:
    void resourceChanged();
    void audioEnabledChanged();
    void serverSessionManagerChanged();

private:
    void setRawResource(const QnResource* value);
    QnResource* rawResource() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::mobile
