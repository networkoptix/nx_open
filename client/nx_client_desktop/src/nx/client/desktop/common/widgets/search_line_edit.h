#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>
#include <QtWidgets/QStyleOption>

#include <nx/utils/pending_operation.h>

class QLineEdit;

namespace nx {
namespace client {
namespace desktop {

class SearchLineEdit: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int textChangedSignalFilterMs READ textChangedSignalFilterMs WRITE setTextChangedSignalFilterMs)

public:
    explicit SearchLineEdit(QWidget* parent = nullptr);

    inline QLineEdit* lineEdit() const { return m_lineEdit; }

    QSize sizeHint() const;
    QString text() const;

    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;

    int textChangedSignalFilterMs() const;
    void setTextChangedSignalFilterMs(int filterMs);

    void clear();

signals:
    void textChanged(const QString& text);
    void escKeyPressed();
    void enterKeyPressed();
    void enabledChanged();

protected:
    void resizeEvent(QResizeEvent* event);
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    bool event(QEvent* event) override;
    void changeEvent(QEvent* event);

protected:
    void initStyleOption(QStyleOptionFrameV2* option) const;

private:
    QLineEdit* const m_lineEdit = nullptr;
    const QScopedPointer<utils::PendingOperation> m_emitTextChanged;
};

} // namespace desktop
} // namespace client
} // namespace nx
