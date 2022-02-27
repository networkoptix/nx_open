// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>

namespace nx::vms::client::desktop {

/**
A combo box class that allows selection of an icon from a multi-column dropdown list.
*/
class IconComboBox: public QComboBox
{
    Q_OBJECT
    using base_type = QComboBox;

public:
    IconComboBox(QWidget* parent = nullptr);
    virtual ~IconComboBox();

    /**
    Specifies icons to choose from. All icons must reside
    in the same resource folder and have the same extension.
    Icons will be loaded from qnSkin by full names computed as
        "path + '/' + names[i] + extension".
    */
    void setIcons(const QString& path, const QStringList& names,
        const QString& extension = lit(".png"));
    void setPixmaps(const QString& path, const QStringList& names,
        const QString& extension = lit(".png"));

    void setIcons(const QVector<QPair<QString, QIcon>>& icons);
    void setPixmaps(const QVector<QPair<QString, QPixmap>>& pixmaps);

    /**
    Number of displayed columns, 0 means it is chosen automatically (default).
    */
    int columnCount() const;
    void setColumnCount(int count);

    /**
    Maximum number of displayed rows before vertical scroll bar appears.
    Replaces QComboBox::maxVisibleItems
    */
    int maxVisibleRows() const;
    void setMaxVisibleRows(int count);

    /**
    Dropdown list item size. If not set, iconSize is used.
    */
    QSize itemSize() const;
    void setItemSize(const QSize& size);

    /**
    Currently selected icon, identified by name.
    */
    QString currentIcon() const;
    void setCurrentIcon(const QString& name);

public:
    virtual void showPopup() override;

protected:
    using base_type::currentText;
    using base_type::setCurrentText;
    using base_type::maxVisibleItems;
    using base_type::setMaxVisibleItems;

    virtual void adjustPopupParameters();

private:
    int m_columnCount = 0; //< Automatic by default.
    int m_maxVisibleRows = 5;

    class Delegate;
    const QScopedPointer<Delegate> m_delegate;
};

} // namespace nx::vms::client::desktop
