#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

class QnTreeView;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

// One-row, two-column tree view that displays a text item and a checkbox.
// Intended to be used as a table-like "All Items" selector above a table of actual items.
class CheckableLineWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit CheckableLineWidget(QWidget* parent = nullptr);
    virtual ~CheckableLineWidget() override;

    QnTreeView* view() const;

    QString text() const;
    void setText(const QString& value);

    QIcon icon() const;
    void setIcon(const QIcon& value);

    bool checked() const;
    void setChecked(bool value);
    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState value);

    QVariant data(int role) const;
    bool setData(const QVariant& value, int role);

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

signals:
    void checkStateChanged();

private:
    class Model;
    struct PrivateData;
    QScopedPointer<PrivateData> m_data;
};


} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
