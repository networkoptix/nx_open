#pragma once

#include <ui/widgets/common/panel.h>

namespace Ui {
class FilenamePanel;
}

namespace nx {
namespace client {
namespace desktop {

class FilenamePanel: public QnPanel
{
    Q_OBJECT
    using base_type = QnPanel;

public:
    explicit FilenamePanel(QWidget *parent = 0);
    virtual ~FilenamePanel() override;

signals:
    bool filenameChanged(const QString& filename);

private:
    QScopedPointer<Ui::FilenamePanel> ui;
};

} // namespace desktop
} // namespace client
} // namespace nx
