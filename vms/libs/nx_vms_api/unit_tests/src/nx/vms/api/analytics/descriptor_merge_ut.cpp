// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QFile>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

class DescriptorMergeTest: public ::testing::Test
{
protected:
    void givenDescriptors(const QString& filePrefix)
    {
        QFile file(NX_FMT(":/descriptor_merge_test/%1_descriptors.json", filePrefix));

        ASSERT_TRUE(file.open(QFile::ReadOnly));
        const QByteArray data = file.readAll();

        QJsonObject fullData;
        ASSERT_TRUE(QJson::deserialize(data, &fullData));
        ASSERT_TRUE(QJson::deserialize(fullData["initial"].toObject(), &m_initialDescriptors));
        ASSERT_TRUE(QJson::deserialize(
            fullData["additional"].toObject(), &m_additionalDescriptors));
        ASSERT_TRUE(QJson::deserialize(fullData["expected"].toObject(), &m_expectedDescriptors));

        ASSERT_FALSE(m_initialDescriptors.isEmpty());
        ASSERT_FALSE(m_additionalDescriptors.isEmpty());
        ASSERT_FALSE(m_expectedDescriptors.isEmpty());
    }

    void afterDescriptorsAreMerged()
    {
        m_initialDescriptors.merge(m_additionalDescriptors);
    }

    void makeSureDescriptorsAreValid()
    {
        ASSERT_EQ(m_initialDescriptors, m_expectedDescriptors);
    }

private:
    Descriptors m_initialDescriptors;
    Descriptors m_additionalDescriptors;
    Descriptors m_expectedDescriptors;
};

TEST_F(DescriptorMergeTest, pluginDescriptors)
{
    givenDescriptors("plugin");
    afterDescriptorsAreMerged();
    makeSureDescriptorsAreValid();
}

TEST_F(DescriptorMergeTest, engineDescriptor)
{
    givenDescriptors("engine");
    afterDescriptorsAreMerged();
    makeSureDescriptorsAreValid();
}

TEST_F(DescriptorMergeTest, groupDescriptor)
{
    givenDescriptors("group");
    afterDescriptorsAreMerged();
    makeSureDescriptorsAreValid();
}

TEST_F(DescriptorMergeTest, eventTypeDescriptor)
{
    givenDescriptors("event_type");
    afterDescriptorsAreMerged();
    makeSureDescriptorsAreValid();
}

TEST_F(DescriptorMergeTest, objectTypeDescriptor)
{
    givenDescriptors("object_type");
    afterDescriptorsAreMerged();
    makeSureDescriptorsAreValid();
}

TEST_F(DescriptorMergeTest, enumTypeDescriptor)
{
    givenDescriptors("enum_type");
    afterDescriptorsAreMerged();
    makeSureDescriptorsAreValid();
}

TEST_F(DescriptorMergeTest, colorTypeDescriptor)
{
    givenDescriptors("color_type");
    afterDescriptorsAreMerged();
    makeSureDescriptorsAreValid();
}

} // namespace nx::vms::api::analytics
