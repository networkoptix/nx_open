#include <gtest/gtest.h>

#include <plugins/resource/iqinvision/iqinvision_resource_searcher.h>

TEST(IqeModelMatch, main)
{
    ASSERT_TRUE(QnPlIqResourceSearcher::isIqeModel("IQ732N"));
    ASSERT_TRUE(QnPlIqResourceSearcher::isIqeModel("IQ865N"));
    ASSERT_TRUE(QnPlIqResourceSearcher::isIqeModel("IQ8712N"));
    ASSERT_TRUE(QnPlIqResourceSearcher::isIqeModel("IQA11N"));
    ASSERT_TRUE(QnPlIqResourceSearcher::isIqeModel("IQeye852"));

    ASSERT_TRUE(QnPlIqResourceSearcher::isIqeModel("IQD65NV"));
    ASSERT_TRUE(QnPlIqResourceSearcher::isIqeModel("IQM62NE"));

    ASSERT_FALSE(QnPlIqResourceSearcher::isIqeModel("IQC03_lyear"));
}
