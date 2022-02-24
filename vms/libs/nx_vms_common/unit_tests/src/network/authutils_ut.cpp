// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <network/authutil.h>
#include <nx/utils/log/log.h>

// This test might be useful to calculate auth query param for manual testing purpuses.
TEST(AuthUtil, DISABLED_auth)
{
    qDebug().noquote().nospace() << "----- ?auth=" << createHttpQueryAuthParam(
        "admin", "qweasd123", "VMS", "GET", "550eacc0c45d8");
}

// TODO: There should be actual authorization tests.
