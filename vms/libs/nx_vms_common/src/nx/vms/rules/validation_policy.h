// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS
#include <QtGui/QValidator>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/common/system_context_aware.h>

class QnResourceAccessSubject;

// User and role validation policy.
class NX_VMS_COMMON_API QnSubjectValidationPolicy: public nx::vms::common::SystemContextAware
{
public:
    explicit QnSubjectValidationPolicy(
        nx::vms::common::SystemContext* systemContext,
        bool allowEmptySelection = false);
    virtual ~QnSubjectValidationPolicy();

    virtual QValidator::State roleValidity(const nx::Uuid& roleId) const = 0;
    virtual bool userValidity(const QnUserResourcePtr& user) const = 0;

    virtual QValidator::State validity(bool allUsers, const QSet<nx::Uuid>& subjects) const;
    virtual QString calculateAlert(bool allUsers, const QSet<nx::Uuid>& subjects) const;

    void analyze(bool allUsers, const QSet<nx::Uuid>& subjects,
        QVector<nx::Uuid>& validRoles,
        QVector<nx::Uuid>& invalidRoles,
        QVector<nx::Uuid>& intermediateRoles,
        QVector<QnUserResourcePtr>& validUsers,
        QVector<QnUserResourcePtr>& invalidUsers) const;

    bool isEmptySelectionAllowed() const;

private:
    const bool m_allowEmptySelection;
};

// Default validation policy: users and roles are always valid.
class NX_VMS_COMMON_API QnDefaultSubjectValidationPolicy: public QnSubjectValidationPolicy
{
    using base_type = QnSubjectValidationPolicy;

public:
    QnDefaultSubjectValidationPolicy(
        nx::vms::common::SystemContext* systemContext,
        bool allowEmptySelection = false);

    virtual QValidator::State roleValidity(const nx::Uuid& roleId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;
};

// Subject validation policy when one global permission is required.
class NX_VMS_COMMON_API QnRequiredAccessRightPolicy: public QnSubjectValidationPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnRequiredAccessRightPolicy)
    using base_type = QnSubjectValidationPolicy;

public:
    explicit QnRequiredAccessRightPolicy(
        nx::vms::common::SystemContext* systemContext,
        nx::vms::api::AccessRight requiredAccessRight,
        bool allowEmptySelection = false);

    virtual QValidator::State roleValidity(const nx::Uuid& groupId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;
    virtual QString calculateAlert(bool allUsers, const QSet<nx::Uuid>& subjects) const override;

    QnSharedResourcePointerList<QnVirtualCameraResource> cameras() const;
    void setCameras(const QnSharedResourcePointerList<QnVirtualCameraResource>& cameras);

private:
    bool isSubjectValid(
        const QnResourceAccessSubject& subject, bool allSelectedCamerasRequired = true) const;
    bool hasAnyCameraPermission(const QnUserResourcePtr& user) const;

private:
    const nx::vms::api::AccessRight m_requiredAccessRight;
    QnSharedResourcePointerList<QnVirtualCameraResource> m_cameras;
};

class NX_VMS_COMMON_API QnLayoutAccessValidationPolicy: public QnSubjectValidationPolicy
{
    using base_type = QnSubjectValidationPolicy;

public:
    using QnSubjectValidationPolicy::QnSubjectValidationPolicy;

    virtual QValidator::State roleValidity(const nx::Uuid& roleId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;

    void setLayout(const QnLayoutResourcePtr& layout);

private:
    QnLayoutResourcePtr m_layout;
};

class NX_VMS_COMMON_API QnCloudUsersValidationPolicy: public QnSubjectValidationPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnCloudUsersValidationPolicy)
    using base_type = QnSubjectValidationPolicy;

public:
    using QnSubjectValidationPolicy::QnSubjectValidationPolicy;

    virtual QValidator::State roleValidity(const nx::Uuid& roleId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;
    virtual QString calculateAlert(bool allUsers, const QSet<nx::Uuid>& subjects) const override;
};

class NX_VMS_COMMON_API QnUserWithEmailValidationPolicy: public QnSubjectValidationPolicy
{
    Q_DECLARE_TR_FUNCTIONS(QnUsersWithEmailValidationPolicy)
    using base_type = QnSubjectValidationPolicy;

public:
    using QnSubjectValidationPolicy::QnSubjectValidationPolicy;

    virtual QValidator::State roleValidity(const nx::Uuid& roleId) const override;
    virtual bool userValidity(const QnUserResourcePtr& user) const override;
};
