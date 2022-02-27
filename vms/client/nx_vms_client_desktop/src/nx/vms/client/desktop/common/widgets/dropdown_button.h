// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <QtWidgets/QPushButton>

/**
* This is a button with dropdown menu acting like a combo box.
* It keeps track of currently selected menu action. Triggering an action makes it current.
* When no action is selected the button displays its own text and icon.
* When some action is selected the button displays text (and optionally icon) from the action.
*/
class DropdownButton: public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(Qt::ItemDataRole buttonTextRole READ buttonTextRole WRITE setButtonTextRole)
    Q_PROPERTY(bool displayActionIcon READ displayActionIcon WRITE setDisplayActionIcon)
    using base_type = QPushButton;

public:
    /* Constructors. Initially create empty dropdown menu. */
    explicit DropdownButton(QWidget* parent = nullptr);
    explicit DropdownButton(const QString &text, QWidget* parent = nullptr);
    DropdownButton(const QIcon& icon, const QString& text, QWidget* parent = nullptr);

    /* Which data from current action determines displayed button text.
     * Only DisplayRole, ToolTipRole, StatusTipRole and WhatsThisRole are accepted. */
    Qt::ItemDataRole buttonTextRole() const;
    void setButtonTextRole(Qt::ItemDataRole role);

    /* Whether the button should display current action icon or always display its own icon. */
    bool displayActionIcon() const;
    void setDisplayActionIcon(bool display);

    /* Total count of actions in the menu. */
    int count() const;

    /* Action by index. */
    QAction* action(int index) const;

    /* Index of action. */
    int indexOf(QAction* action) const;

    /* Currently selected index. */
    int currentIndex() const;
    void setCurrentIndex(int index, bool trigger = true);

    /* Currently selected action. */
    QAction* currentAction() const;
    void setCurrentAction(QAction* action, bool trigger = true);

    /* Whether an action is selectable (not disabled nor a separator nor a sub-menu). */
    static bool isActionSelectable(QAction* action);

    /**
     * Specifies the size behavior.
     * @param enable Adjust the size to fit text. By default (false), the button resizes itself to
     *     the longest action text.
     */
    void setAdjustSize(bool enable);

    virtual QSize sizeHint() const override;

signals:
    /* Emitted after the current index changes. */
    void currentChanged(int index);

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    void actionsUpdated();
    void actionTriggered(QAction* action);
    QString buttonText(QAction* action) const;
    QPoint menuPosition() const;

    using base_type::setMenu; //< hidden: the menu should not be replaced

private:
    int m_currentIndex = -1;
    QAction* m_currentAction = nullptr;
    Qt::ItemDataRole m_buttonTextRole = Qt::DisplayRole;
    bool m_displayActionIcon = false;
    bool m_adjustSize = false;

    Q_DECLARE_PRIVATE(QPushButton); //< for access to sizeHint cache
};
