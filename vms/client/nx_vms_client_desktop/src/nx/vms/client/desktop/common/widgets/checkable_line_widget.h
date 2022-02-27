// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class TreeView;

// One-row, two-column tree view that displays a text item and a checkbox.
// Intended to be used as a table-like "All Items" selector above a table of actual items.
class CheckableLineWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CheckableLineWidget(QWidget* parent = nullptr);
    virtual ~CheckableLineWidget() override;

    QString text() const;
    void setText(const QString& value);

    QIcon icon() const;
    void setIcon(const QIcon& value);

    bool checked() const;
    void setChecked(bool value);
    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState value);

    bool userCheckable() const;
    void setUserCheckable(bool value);

    QVariant data(int role) const;
    bool setData(const QVariant& value, int role);

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    enum Columns //< Information for view item delegate customization.
    {
        NameColumn,
        CheckColumn,
        ColumnCount
    };

    TreeView* view() const;

signals:
    void checkStateChanged(Qt::CheckState newCheckState);

private:
    class Model;
    struct PrivateData;
    QScopedPointer<PrivateData> m_data;
};

} // namespace nx::vms::client::desktop
