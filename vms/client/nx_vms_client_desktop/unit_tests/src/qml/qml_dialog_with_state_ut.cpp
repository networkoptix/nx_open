// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include "qml_test_environment.h"
#include "qml_dialog_with_state_ut.h"

namespace nx::vms::client::desktop {
namespace test {

class QmlDialogWithStateTest: public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_dialog.reset(new TestDialog(m_env.engine()));
        m_dialog->testFixture = this;
        m_savedState = TestDialogState{};
    }

    virtual void TearDown() override
    {
        m_dialog.reset();
    }

    void initDialogWith(int value)
    {
        m_dialog->setValue(value);
        ASSERT_FALSE(m_dialog->isModified());
    }

    void userChangesValue1To(int value)
    {
        m_dialog->value1 = value;
        ASSERT_TRUE(m_dialog->isModified());
    }

    void nonUserDialogUpdateWith(int value)
    {
        m_dialog->updateStateFrom(value);
    }

    void verifyValuesInQml(int value1, int value2)
    {
        ASSERT_EQ(value1, m_dialog->value1);
        ASSERT_EQ(value2, m_dialog->value2);
    }

    void verifyOriginalState(int value1, int value2)
    {
        ASSERT_EQ(value1, m_dialog->originalState().value1);
        ASSERT_EQ(value2, m_dialog->originalState().value2);
    }

    void verifySavedState(int value1, int value2)
    {
        ASSERT_EQ(value1, m_savedState.value1);
        ASSERT_EQ(value2, m_savedState.value2);
    }

    const QmlTestEnvironment m_env;
    std::unique_ptr<TestDialog> m_dialog;
    TestDialogState m_savedState;
};

TestDialog::TestDialog(QQmlEngine* engine):
    base_type(nullptr, "", engine)
{
    setData(R"(
        import QtQuick 2.15
        import Nx.Dialogs 1.0

        DialogWithState
        {
            // State properties.
            property int value1
            property int value2
        }
        )", QUrl("qrc:/"));

    value1 = QmlProperty<int>(rootObjectHolder()->object(), "value1");
    value2 = QmlProperty<int>(rootObjectHolder()->object(), "value2");
    modified = QmlProperty<bool>(rootObjectHolder()->object(), "modified");
}

void TestDialog::setValue(int value)
{
    createStateFrom(value);
}

TestDialogState TestDialog::createState(const int& value)
{
    // For incoming value generate the state {value, value + 1}.
    TestDialogState state;
    state.value1 = value;
    state.value2 = value + 1;
    return state;
}

void TestDialog::saveState(const TestDialogState& state)
{
    testFixture->m_savedState = state;

    // Notify the dialog that state save is successfully completed.
    saveStateComplete(state);
}

TEST_F(QmlDialogWithStateTest, stateCreation)
{
    // Initialize dialog with state {1, 2}.
    initDialogWith(1);

    verifyValuesInQml(1, 2);
    verifyOriginalState(1, 2);
}

TEST_F(QmlDialogWithStateTest, stateModifyInternal)
{
    // Initialize dialog with state {1, 2}.
    initDialogWith(1);
    verifyValuesInQml(1, 2);

    // Simulate user changes to the state: {1, 2} -> {3, 2}.
    userChangesValue1To(3);

    // Check that dialog variables are set to the new state.
    verifyValuesInQml(3, 2);

    // Verify original state did not change.
    verifyOriginalState(1, 2);
}

TEST_F(QmlDialogWithStateTest, stateModifyExternal)
{
    // Initialize dialog with the state {1, 2}.
    initDialogWith(1);
    verifyValuesInQml(1, 2);

    // Simulate non-user changes to the state: {1, 2} -> {10, 11}.
    nonUserDialogUpdateWith(10);
    ASSERT_FALSE(m_dialog->isModified());

    // Check that dialog variables are set to the new state.
    verifyValuesInQml(10, 11);
    verifyOriginalState(10, 11);
}

TEST_F(QmlDialogWithStateTest, stateMerge)
{
    // Initialize dialog with the state {1, 2}.
    initDialogWith(1);
    verifyValuesInQml(1, 2);

    // Simulate user changes to the state: {1, 2} -> {3, 2}.
    userChangesValue1To(3);

    // Simulate non-user changes to the original state: {1, 2} -> {10, 11}.
    nonUserDialogUpdateWith(10);
    ASSERT_TRUE(m_dialog->isModified());

    // Check that dialog variables are set to the merged state {3, 11}.
    verifyValuesInQml(3, 11);

    // Check that original state is replaced with {10, 11}.
    verifyOriginalState(10, 11);
}

TEST_F(QmlDialogWithStateTest, stateSave)
{
    // Initialize dialog with the state {1, 2}.
    initDialogWith(1);
    verifyValuesInQml(1, 2);

    // Simulate user changes to the state: {1, 2} -> {3, 2}.
    userChangesValue1To(3);

    // User accepts the changes.
    QMetaObject::invokeMethod(m_dialog->window(), "accept");

    ASSERT_FALSE(m_dialog->isModified());

    // Check that modified state {3, 2} is saved.
    verifySavedState(3, 2);

    // Original state is also updated.
    verifyOriginalState(3, 2);
}

} // namespace test
} // namespace nx::vms::client::desktop
