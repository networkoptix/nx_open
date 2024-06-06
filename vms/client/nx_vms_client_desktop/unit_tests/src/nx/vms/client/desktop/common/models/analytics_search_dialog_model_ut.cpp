// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QPointer>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QStandardItemModel>
#include <QtTest/QAbstractItemModelTester>

#include <nx/utils/debug_helpers/model_transaction_checker.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/analytics/analytics_filter_model.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/desktop/analytics/analytics_dialog_table_model.h>
#include <nx/vms/client/desktop/analytics/attribute_display_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>

namespace nx::vms::client::desktop {
namespace test {

class AttributeDisplayManager: public analytics::taxonomy::AttributeDisplayManager
{
    using base_type = analytics::taxonomy::AttributeDisplayManager;

public:
    AttributeDisplayManager(core::analytics::taxonomy::AnalyticsFilterModel* filterModel)
        :
        base_type(AttributeDisplayManager::Mode::tableView, filterModel)
    {}

    virtual QStringList attributesForObjectType(const QStringList&) const override
    {
        return {"Attribute 1", "Attribute 2", "Attribute 3"};
    }

    virtual QSet<QString> visibleAttributes() const override
    {
        const auto attributes = attributesForObjectType({});
        return base_type::visibleAttributes() +
            QSet(attributes.cbegin(), attributes.cend());
    }

    virtual QStringList attributes() const override
    {
        return base_type::attributes() + attributesForObjectType({});
    }
};

class SourceModel: public QStandardItemModel
{
    using base_type = QStandardItemModel;

public:
    SourceModel(
        AttributeDisplayManager* attributeManager, QObject* parent = nullptr)
        :
        base_type(parent),
        m_attributeManager(attributeManager)
    {}

    virtual QVariant data(const QModelIndex& index, int role) const override
    {
        using namespace core::analytics;

        const int row = index.row();
        switch (role)
        {
            case core::TimestampTextRole:
                return m_attributeManager->displayName(analytics::taxonomy::kDateTimeAttributeName)
                    + " "
                    + QString::number(row);

            case core::DisplayedResourceListRole:
                return m_attributeManager->displayName(analytics::taxonomy::kCameraAttributeName)
                    + " "
                    + QString::number(row);

            case Qt::DisplayRole:
                return
                    m_attributeManager->displayName(analytics::taxonomy::kObjectTypeAttributeName)
                    + " "
                    + QString::number(row);

            case core::AnalyticsAttributesRole:
            {
                AttributeList result;
                for (auto attributeName: m_attributeManager->attributesForObjectType({}))
                {
                    result << Attribute{
                        .id = attributeName,
                        .displayedValues = {attributeName + " " + QString::number(row)}};
                }
                return QVariant::fromValue(result);
            }
        };

        return base_type::data(index, role);
    }

private:
    AttributeDisplayManager* m_attributeManager;
};

class AnalyticsDialogTableModelTest: public ContextBasedTest
{
public:
    virtual void SetUp() override
    {
        using namespace analytics::taxonomy;

        m_filterModel.reset(new core::analytics::taxonomy::AnalyticsFilterModel());
        m_attributeManager.reset(new AttributeDisplayManager(m_filterModel.get()));
        for (auto field: m_attributeManager->attributes())
            m_attributeManager->setVisible(field, true);

        m_sourceModel.reset(new SourceModel(m_attributeManager.get()));
        for (int i = 1; i <= 5; ++i)
            m_sourceModel->appendRow(new QStandardItem("Object " + QString::number(i)));

        m_testModel.reset(new AnalyticsDialogTableModel());
        m_testModel->setSourceModel(m_sourceModel.get());
        m_testModel->setAttributeManager(m_attributeManager.get());

        m_modelTester.reset(new QAbstractItemModelTester(m_testModel.get(),
            QAbstractItemModelTester::FailureReportingMode::Fatal));

        m_transactionChecker = nx::utils::ModelTransactionChecker::install(m_testModel.get());
    }

    virtual void TearDown() override
    {
        m_modelTester.reset();
        m_testModel.reset();
        m_sourceModel.reset();
        m_attributeManager.reset();
        m_filterModel.reset();
    }

    void getHeaderData()
    {
        m_headerData.clear();
        for (int col = 0; col < m_testModel->columnCount({}); ++col)
        {
            m_headerData.append(
                m_testModel->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString());
        }
    }

public:
    QStringList m_headerData;
    std::unique_ptr<SourceModel> m_sourceModel;
    std::unique_ptr<analytics::taxonomy::AnalyticsDialogTableModel> m_testModel;
    std::unique_ptr<QAbstractItemModelTester> m_modelTester;
    std::unique_ptr<core::analytics::taxonomy::AnalyticsFilterModel> m_filterModel;
    std::unique_ptr<AttributeDisplayManager> m_attributeManager;
    QPointer<nx::utils::ModelTransactionChecker> m_transactionChecker;
};

// ------------------------------------------------------------------------------------------------
//

TEST_F(AnalyticsDialogTableModelTest, invalidIndex)
{
    auto index = m_testModel->index(m_sourceModel->rowCount(), 0);
    ASSERT_EQ(index, QModelIndex());
}

TEST_F(AnalyticsDialogTableModelTest, columns)
{
    getHeaderData();
    for (int row = 0; row < m_testModel->rowCount({}); ++row)
    {
        for (int col = 0; col < m_testModel->columnCount({}); ++col)
        {
            auto result = m_testModel->data(m_testModel->index(row, col), Qt::DisplayRole);
            ASSERT_EQ(result.toString(), m_headerData[col] + " " + QString::number(row));
        }
    }
}

TEST_F(AnalyticsDialogTableModelTest, insertRows)
{
    int rowsBefore = m_testModel->rowCount({});
    m_sourceModel->appendRow(new QStandardItem("New line 1"));
    m_sourceModel->appendRow(new QStandardItem("New line 2"));
    m_sourceModel->appendRow(new QStandardItem("New line 3"));
    int rowsAfter = m_testModel->rowCount({});
    ASSERT_EQ(rowsAfter, rowsBefore + 3);
}

TEST_F(AnalyticsDialogTableModelTest, removeRows)
{
    int rowsBefore = m_testModel->rowCount({});
    ASSERT_GE(rowsBefore, 2);
    m_sourceModel->removeRows(3, 2);
    int rowsAfter = m_testModel->rowCount({});
    ASSERT_EQ(rowsAfter, rowsBefore - 2);
}

TEST_F(AnalyticsDialogTableModelTest, changeColumnsVisibility)
{
    getHeaderData();
    const int columnsBefore = m_testModel->columnCount({});

    // Trying to hide Date/Time column.
    m_attributeManager->setVisible(analytics::taxonomy::kDateTimeAttributeName, false);
    // It is prohibited to change visibility of the Date/Time column, so it won't take an effect.
    ASSERT_EQ(m_testModel->columnCount({}), columnsBefore);

    for (int row = 0; row < m_testModel->rowCount({}); ++row)
    {
        for (int col = 0; col < m_headerData.count(); ++ col)
        {
            auto result = m_testModel->data(m_testModel->index(row, col), Qt::DisplayRole);
            ASSERT_EQ(result.toString(), m_headerData[col] + " " + QString::number(row));
        }
    }

    // Hiding the Camera column.
    m_attributeManager->setVisible(analytics::taxonomy::kCameraAttributeName, false);
    ASSERT_EQ(m_testModel->columnCount({}), columnsBefore - 1);

    m_headerData.removeOne(
        m_attributeManager->displayName(analytics::taxonomy::kCameraAttributeName));
    for (int row = 0; row < m_testModel->rowCount({}); ++row)
    {
        for (int col = 0; col < m_headerData.count(); ++col)
        {
            auto result = m_testModel->data(m_testModel->index(row, col), Qt::DisplayRole);
            ASSERT_EQ(result.toString(), m_headerData[col] + " " + QString::number(row));
        }
    }

    m_attributeManager->setVisible(analytics::taxonomy::kObjectTypeAttributeName, false);
    ASSERT_EQ(m_testModel->columnCount({}), columnsBefore - 2);
    m_attributeManager->setVisible(analytics::taxonomy::kCameraAttributeName, true);
    ASSERT_EQ(m_testModel->columnCount({}), columnsBefore - 1);
    m_attributeManager->setVisible(analytics::taxonomy::kObjectTypeAttributeName, true);
    ASSERT_EQ(m_testModel->columnCount({}), columnsBefore);
}

} // namespace test
} // namespace nx::vms::client::desktop
