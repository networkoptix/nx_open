// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/webpage_resource.h>
#include <nx/vms/client/desktop/manual_device_addition/dialog/private/current_system_servers.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <ui/dialogs/common/button_box_dialog.h>

namespace nx::vms::api { enum class WebPageSubtype; }

namespace Ui { class WebpageDialog; }

namespace nx::vms::client::desktop {

class QnWebpageDialog:
    public QnButtonBoxDialog,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    enum EditMode
    {
        AddPage,  //< Dialog title displays "Add [Proxied] Web Page".
        EditPage, //< Dialog title displays "Edit [Proxied] Web Page".
    };

public:
    QnWebpageDialog(
        SystemContext* systemContext,
        QWidget* parent,
        EditMode editMode);

    virtual ~QnWebpageDialog();

    QString name() const;
    void setName(const QString& name);
    nx::Uuid proxyId() const;

    QUrl url() const;
    void setUrl(const QUrl& url);
    void setProxyId(nx::Uuid id);

    nx::vms::api::WebPageSubtype subtype() const;
    void setSubtype(nx::vms::api::WebPageSubtype value);

    QStringList proxyDomainAllowList() const;
    void setProxyDomainAllowList(const QStringList& allowList);

    std::chrono::seconds refreshInterval() const;
    void setRefreshInterval(std::chrono::seconds interval);

    void setResourceId(nx::Uuid id);

    bool isCertificateCheckEnabled() const;
    void setCertificateCheckEnabled(bool value);

    bool isOpenInWindow() const;
    void setOpenInWindow(bool value);

    virtual void accept() override;

    void submitData(const QnWebPageResourcePtr& webPageResource) const;

private:
    void updateTitle();
    void updateText();
    void updateWarningLabel();
    QString getTitle(QnWebPageResource::Options flags);

private:
    CurrentSystemServers m_serversWatcher;
    nx::vms::api::WebPageSubtype m_subtype = nx::vms::api::WebPageSubtype::none;
    EditMode m_editMode = AddPage;

    QScopedPointer<Ui::WebpageDialog> ui;
    bool m_initialProxyAll = false;
    nx::Uuid m_initialProxyId;
    nx::Uuid m_resourceId;
};

} // namespace nx::vms::client::desktop
