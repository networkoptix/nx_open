#pragma once

#include <common/common_globals.h>
#include <core/resource/client_resource_fwd.h>
#include <core/resource/layout_resource.h>
#include <utils/crypt/encryptable.h>

class NX_VMS_CLIENT_DESKTOP_API QnFileLayoutResource:
    public QnLayoutResource,
    public nx::utils::Encryptable
{
    Q_OBJECT

    using base_type = QnLayoutResource;
public:
    QnFileLayoutResource(QnCommonModule* commonModule = nullptr);

    virtual QString getUniqueId() const override;

    virtual Qn::ResourceStatus getStatus() const override;
    virtual void setStatus(Qn::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

    virtual bool isFile() const override;

    /** Sets encryption flag. */
    void setIsEncrypted(bool encrypted);
    /** Returns true if the entity is actually encrypted. */
    virtual bool isEncrypted() const override;
    /** Returns true if the entity is encrypted and no valid password is provided. */
    using Encryptable::requiresPassword;
    /** Attempts to set a password for opening existing entity. */
    virtual bool usePasswordToRead(const QString& password) override;
    /** Returns password */
    virtual QString password() const override;

signals:
    void passwordChanged(const QnResourcePtr &resource);
    void readOnlyChanged(const QnResourcePtr &resource);

protected:
    void updateInternal(const QnResourcePtr& other, Qn::NotifierList& notifiers) override;

private:
    Qn::ResourceStatus m_localStatus = Qn::Online;
    bool m_isEncrypted = false;
    QString m_password;
    bool m_isReadOnly = false;
};

Q_DECLARE_METATYPE(QnFileLayoutResourcePtr);
Q_DECLARE_METATYPE(QnFileLayoutResourceList);
