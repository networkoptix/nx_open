#pragma once

#include <QtWidgets/QWidget>

class QMenu;
class QLineEdit;
class QPushButton;

namespace nx {
namespace client {
namespace desktop {

class SelectableTextButton;
class CheckableMenu;

class SearchEdit: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(int selectedTagIndex READ selectedTagIndex NOTIFY selectedTagIndexChanged)

public:
    SearchEdit(QWidget* parent = nullptr);

    virtual ~SearchEdit() override;

    QString text() const;
    void setText(const QString& value);

    void clear();

    QString placeholderText() const;
    void setPlaceholderText(const QString& value);

    QStringList tagsList() const;
    void setTags(const QStringList& value);

    int selectedTagIndex() const;

    void setClearingTagIndex(int index);
    int clearingTagIndex() const;

public:
    QSize sizeHint() const override;
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const override;

signals:
    void textChanged();
    void editingFinished();
    void selectedTagIndexChanged();

    void enterPressed();
    void ctrlEnterPressed();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;

    bool event(QEvent* event) override;

private:
    void setupMenuButton();

    void updatePalette();
    void setSelectedTagIndex(int value);
    void handleTagButtonStateChanged();
    void updateTagButton();
    void setHovered(bool value);
    void setButtonHovered(bool value);

private:
    struct Private;
    const QScopedPointer<Private> d;

    QStringList m_tags;
};

} // namespace desktop
} // namespace client
} // namespace nx
