// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_with_state.h>

namespace nx::vms::client::desktop::test {

class QmlDialogWithStateTest;

struct TestDialogState
{
    Q_GADGET

    Q_PROPERTY(int value1 MEMBER value1)
    Q_PROPERTY(int value2 MEMBER value2)

public:
    bool operator==(const TestDialogState&) const = default;

    int value1 = 0;
    int value2 = 0;
};

class TestDialog: public QmlDialogWithState<TestDialogState, int>
{
    using base_type = QmlDialogWithState<TestDialogState, int>;

    Q_OBJECT

    friend class QmlDialogWithStateTest;

public:
    TestDialog(QQmlEngine* engine);

    void setValue(int value);

    bool isModified() const { return modified; }

protected:
    virtual TestDialogState createState(const int& value) override;
    virtual void saveState(const TestDialogState& state) override;

private:
    QmlProperty<int> value1;
    QmlProperty<int> value2;
    QmlProperty<bool> modified;

    QmlDialogWithStateTest* testFixture = nullptr;
};

} // namespace nx::vms::client::desktop::test
