#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class FilteredResourceSelectionWidget; }

namespace nx::vms::client::desktop {

namespace node_view { class ResourceSelectionNodeView; }

class FilteredResourceSelectionWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

    Q_PROPERTY(bool detailsVisible READ detailsVisible WRITE setDetailsVisible
        NOTIFY detailsVisibleChanged)
    Q_PROPERTY(QString invalidMessage READ invalidMessage WRITE setInvalidMessage
        NOTIFY invalidMessageChanged)
    Q_PROPERTY(QString infoMessage READ infoMessage WRITE setInfoMessage)

public:
    FilteredResourceSelectionWidget(QWidget* parent = nullptr);
    virtual ~FilteredResourceSelectionWidget() override;

    node_view::ResourceSelectionNodeView* view();

    QString invalidMessage() const;
    void setInvalidMessage(const QString& text);
    void clearInvalidMessage();

    QString infoMessage() const;
    void setInfoMessage(const QString& text);
    void clearInfoMessage();

    bool detailsVisible() const;
    void setDetailsVisible(bool visible);

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void detailsVisibleChanged();
    void invalidMessageChanged();

private:
    const QScopedPointer<Ui::FilteredResourceSelectionWidget> ui;
};

} // namespace nx::vms::client::desktop
