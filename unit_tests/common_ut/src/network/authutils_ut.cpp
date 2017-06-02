#include <gtest/gtest.h>

#include <network/authutil.h>
#include <nx/utils/log/log.h>

// This test might be useful to calculate auth query param for manual testing purpuses.
TEST(AuthUtil, DISABLED_auth)
{
    qDebug().noquote().nospace() << "----- ?auth=" << createHttpQueryAuthParam(
        lit("admin"), lit("qweasd123"), lit("VMS"), "GET", "550eacc0c45d8");
}

// TODO: There should be actual authorization tests.
