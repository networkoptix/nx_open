#pragma once

#include <memory>

#include <gtest/gtest.h>

#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <media_server/media_server_module.h>

class MediaServerModuleFixture:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    MediaServerModuleFixture();

    QnMediaServerModule& serverModule();

protected:
    virtual void SetUp() override;
    virtual void TearDown() override;

private:
    std::unique_ptr<QnMediaServerModule> m_serverModule;
};
