// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/user_group_data.h>

/**
* This class represents subject of resource access - user or user role.
*/
class NX_VMS_COMMON_API QnResourceAccessSubject final
{
public:
    QnResourceAccessSubject();
    QnResourceAccessSubject(const QnUserResourcePtr& user);
    QnResourceAccessSubject(const nx::vms::api::UserGroupData& role);
    QnResourceAccessSubject(const QnResourceAccessSubject& other);

    const QnUserResourcePtr& user() const { return m_user; }

    bool isValid() const {return !m_id.isNull(); }
    bool isUser() const { return !m_user.isNull(); }
    bool isRole() const { return m_user.isNull(); }

    const QnUuid& id() const { return m_id; }
    QString toString() const;

    void operator=(const QnResourceAccessSubject& other);
    bool operator==(const QnResourceAccessSubject& other) const;
    bool operator!=(const QnResourceAccessSubject& other) const;
    bool operator<(const QnResourceAccessSubject& other) const;
    bool operator>(const QnResourceAccessSubject& other) const;

private:
    QnUserResourcePtr m_user;
    QnUuid m_id;
};

NX_VMS_COMMON_API QDebug operator<<(QDebug dbg, const QnResourceAccessSubject& subject);

namespace std {

template<>
struct hash<QnResourceAccessSubject>
{
    size_t operator()(const QnResourceAccessSubject& s) const { return hash<QnUuid>()(s.id()); }
};

} // namepsace std
