// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate_selection_dialog.h"
#include "ui_certificate_selection_dialog.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtQml/QQmlListReference>
#include <QtWidgets/QPushButton>

#include <nx/branding.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>

namespace nx::vms::client::desktop {

namespace {

class CertificatesModel: public QAbstractTableModel
{
    Q_DECLARE_TR_FUNCTIONS(CertificatesModel)
    using base_type = QAbstractTableModel;

    enum Column
    {
        SubjectColumn,
        IssuerColumn,
        ExpiresColumn,
        ColumnCount
    };

public:
    CertificatesModel(
        QObject* parent,
        const QQmlListReference& certificates)
        :
        base_type(parent),
        m_certificates(certificates)
    {}

    int rowCount([[maybe_unused]] const QModelIndex& parent = QModelIndex()) const override
    {
        return m_certificates.count();
    }

    int columnCount([[maybe_unused]] const QModelIndex& parent = QModelIndex()) const override
    {
        return ColumnCount;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid())
            return QVariant();

        switch (index.column())
        {
            case SubjectColumn:
                if (role != Qt::DisplayRole)
                    return QVariant();
                return m_certificates.at(index.row())->property("subject");

            case IssuerColumn:
                if (role != Qt::DisplayRole)
                    return QVariant();
                return m_certificates.at(index.row())->property("issuer");

            case ExpiresColumn:
            {
                if (role != Qt::DisplayRole && role != Qt::ForegroundRole)
                    return QVariant();

                const auto certificate = m_certificates.at(index.row());
                const QDateTime expiryDate = certificate->property("expiryDate").toDateTime();

                if (role == Qt::DisplayRole)
                    return expiryDate.toString("dd.MM.yyyy");

                return expiryDate < QDateTime::currentDateTime()
                    ? QVariant(core::colorTheme()->color("red_l2"))
                    : QVariant();
            }

            default:
                break;
        }

        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        {
            switch (section)
            {
                case SubjectColumn:
                    return tr("Subject");
                case IssuerColumn:
                    return tr("Issuer");
                case ExpiresColumn:
                    return tr("Expires");
                default:
                    break;
            }
        }
        return QVariant();
    }

private:
    QQmlListReference m_certificates;
};

} // namespace

CertificateSelectionDialog::CertificateSelectionDialog(
    QWidget* parent,
    const QUrl& hostUrl,
    const QQmlListReference& certificates)
    :
    base_type(parent),
    ui(new Ui::CertificateSelectionDialog())
{
    ui->setupUi(this);

    ui->captionLabel->setStyleSheet(
        QString("QLabel { color: %1; }").arg(core::colorTheme()->color("light10").name()));

    setWindowTitle(nx::branding::vmsName());
    ui->textLabel->setText(
        //: %1 here is a host name for which you provide a certificate.
        tr("Select a certificate to authenticate yourself to %1:").arg(hostUrl.host()));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Select"));

    // Set dialog icon.
    const auto pixmap =
        core::Skin::maximumSizePixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning));
    ui->iconLabel->setVisible(!pixmap.isNull());
    ui->iconLabel->setPixmap(pixmap);
    ui->iconLabel->resize(pixmap.size());

    const auto model = new CertificatesModel(this, certificates);
    ui->certificatesTable->setModel(model);

    ui->certificatesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    if (NX_ASSERT(model->rowCount() > 0))
        ui->certificatesTable->selectRow(0);

    // Always maintain row selection.
    connect(ui->certificatesTable->selectionModel(), &QItemSelectionModel::selectionChanged,
        this,
        [this](const QItemSelection& selected, const QItemSelection& deselected)
        {
            // Select previously selected row if no row is selected.
            if (!selected.empty())
                return;

            ui->certificatesTable->selectRow(deselected.indexes().takeFirst().row());
        });
}

int CertificateSelectionDialog::selectedIndex() const
{
    return ui->certificatesTable->currentIndex().row();
}

CertificateSelectionDialog::~CertificateSelectionDialog()
{
}

} // namespace nx::vms::client::desktop
