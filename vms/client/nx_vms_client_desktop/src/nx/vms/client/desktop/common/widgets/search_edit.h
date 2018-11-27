#pragma once

#include <QtWidgets/QWidget>
#include <nx/vms/client/desktop/resource_views/data/node_type.h>

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

    void setFilterOptionsSource(std::function<QMenu*()> filterMenuCreator,
        std::function<QString(ResourceTreeNodeType)> filterNameProvider);
    std::optional<ResourceTreeNodeType> currentFilter() const;

    bool focused() const;

public:
    QSize sizeHint() const override;
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const override;

signals:
    void textChanged();
    void editingFinished();
    void currentFilterChanged();
    void enterPressed();
    void ctrlEnterPressed();
    void focusedChanged();
    void filteringChanged();

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
    void setCurrentFilter(std::optional<ResourceTreeNodeType> allowedNodeType);
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
