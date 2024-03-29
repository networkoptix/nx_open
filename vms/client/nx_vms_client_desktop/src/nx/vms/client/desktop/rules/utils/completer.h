// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTextEdit>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop::rules {
class EventParametersModel;
class Completer: public QObject
{
    Q_OBJECT

public:
    Completer(const QStringList& words, QLineEdit* lineEdit, QObject* parent = nullptr);
    Completer(const QStringList& words, QTextEdit* textEdit, QObject* parent = nullptr);
    Completer(EventParametersModel* model, QTextEdit* textEdit, QObject* parent = nullptr);
    virtual ~Completer() override;

    /**
     * Takes ownership of model. Deletes previously used model.
     */
    void setModel(EventParametersModel* model);
    bool containsElement(const QString& element);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::rules
