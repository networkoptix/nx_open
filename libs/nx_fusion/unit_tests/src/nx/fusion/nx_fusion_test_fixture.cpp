#include "nx_fusion_test_fixture.h"

#include <nx/fusion/model_functions.h>

void QnFusionTestFixture::SetUp()
{
}

void QnFusionTestFixture::TearDown()
{
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx, TestFlag,
(nx::Flag0, "Flag0")
(nx::Flag1, "Flag1")
(nx::Flag2, "Flag2")
(nx::Flag4, "Flag4")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx, TestFlags,
(nx::Flag0, "Flag0")
(nx::Flag1, "Flag1")
(nx::Flag2, "Flag2")
(nx::Flag4, "Flag4")
)
