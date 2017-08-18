#include "filename_panel.h"
#include "ui_filename_panel.h"

namespace nx {
namespace client {
namespace desktop {

FilenamePanel::FilenamePanel(QWidget* parent):
    base_type(parent),
    ui(new Ui::FilenamePanel)
{
    ui->setupUi(this);

    connect(ui->filenameLineEdit, &QLineEdit::textChanged, this, &FilenamePanel::filenameChanged);
}

FilenamePanel::~FilenamePanel()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
