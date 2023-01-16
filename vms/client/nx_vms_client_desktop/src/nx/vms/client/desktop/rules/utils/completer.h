// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCompleter>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTextEdit>

namespace nx::vms::client::desktop::rules {

class Completer: public QObject
{
    Q_OBJECT

public:
    Completer(const QStringList& words, QLineEdit* lineEdit, QObject* parent = nullptr);
    Completer(const QStringList& words, QTextEdit* textEdit, QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QCompleter m_completer;

    struct CurrentWord
    {
        int begin{0};
        int length{0};
    };
    CurrentWord m_currentWord;
    bool m_isCompletingInProgress{false};

    void updateCompleter(int cursorPosition, const QString& text);
    void replaceCurrentWordWithCompleted(QString& text, const QString& completedText) const;
};

} // namespace nx::vms::client::desktop::rules
