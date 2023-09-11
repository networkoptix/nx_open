// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/fusion/model_functions_fwd.h>

// TODO: #sivanov Remove this class in favor of std::optional<bool>.
class NX_VMS_COMMON_API QnOptionalBool
{
public:
    QnOptionalBool();
    QnOptionalBool(bool value);

    bool value() const;
    void setValue(bool value);

    bool isDefined() const;
    void undefine();

    bool operator ==(const QnOptionalBool &other) const;
private:
    /* Should be used with care. boost::tribool is much better but is not currently included in our distribution. */
    std::optional<bool> m_value;
};

QN_FUSION_DECLARE_FUNCTIONS(QnOptionalBool, (lexical));
