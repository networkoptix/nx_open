#pragma once

#include <QtWidgets/QWidget>

class QMenu;
class QLineEdit;
class QPushButton;

namespace nx::vms::client::desktop {

class SelectableTextButton;
class CheckableMenu;

class SearchEdit: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)

public:
    SearchEdit(QWidget* parent = nullptr);

    virtual ~SearchEdit() override;

    QString text() const;
    void setText(const QString& value);

    void clear();

    QString placeholderText() const;
    void setPlaceholderText(const QString& value);

    bool isMenuEnabled() const;
    void setMenuEnabled(bool enabled);

    QVariant currentTagData() const;
    void setCurrentTagData(const QVariant& tagData);

    /**
    * SearchEdit control can display discardable badge with certain meaning and description.
    *     It may be assigned by both picking some option from context menu or directly calling
    *     setCurrentTagData method.
    *
    * @param tagMenuCreator Factory functor that returns pointer to QMenu instance. Picking one of
    *     menu actions will cause setCurrentTagData method call parametrized with menu action data.
    *
    * @param tagNameProvider Functor that provides descriptive name for tag badge based on value
    *     returned by currentTagData method.
    */
    using MenuCreator = std::function<QMenu*(QWidget* parent)>;
    void setTagOptionsSource(MenuCreator tagMenuCreator,
        std::function<QString(const QVariant&)> tagNameProvider);

    bool focused() const;

public:
    QSize sizeHint() const override;
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const override;

signals:
    void textChanged();
    void editingFinished();
    void currentTagDataChanged();
    void enterPressed();
    void ctrlEnterPressed();
    void focusedChanged();
    void filteringChanged();

protected:
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
    void handleTagButtonStateChanged();
    void updateTagButton();
    void setHovered(bool value);
    void setButtonHovered(bool value);
    void updateMenuButtonIcon();
    void showFilterMenu();
    void setFocused(bool value);
    void updateFocused();

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
