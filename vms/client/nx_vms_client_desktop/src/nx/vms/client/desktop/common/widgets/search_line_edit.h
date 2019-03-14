#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>
#include <QtWidgets/QStyleOption>

class QLineEdit;

namespace nx::utils { class PendingOperation; }

namespace nx::vms::client::desktop {

class SearchLineEdit: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int textChangedSignalFilterMs READ textChangedSignalFilterMs WRITE setTextChangedSignalFilterMs)

public:
    explicit SearchLineEdit(QWidget* parent = nullptr);
    virtual ~SearchLineEdit() override;

    inline QLineEdit* lineEdit() const { return m_lineEdit; }

    QSize sizeHint() const override;
    QString text() const;

    QVariant inputMethodQuery(Qt::InputMethodQuery property) const override;

    int textChangedSignalFilterMs() const;
    void setTextChangedSignalFilterMs(int filterMs);

    void clear();

    void setGlassVisible(bool visible);

signals:
    void textChanged(const QString& text);
    void escKeyPressed();
    void enterKeyPressed();
    void enabledChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    bool event(QEvent* event) override;
    void changeEvent(QEvent* event) override;

protected:
    void initStyleOption(QStyleOptionFrame* option) const;

private:
    QAction* m_glassIcon = nullptr;
    QLineEdit* const m_lineEdit = nullptr;
    const QScopedPointer<utils::PendingOperation> m_emitTextChanged;
};

} // namespace nx::vms::client::desktop
