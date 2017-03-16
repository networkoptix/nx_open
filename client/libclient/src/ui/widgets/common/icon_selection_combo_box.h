#pragma once

#include <QtWidgets/QComboBox>

class QnIconSelectionComboBox: public QComboBox
{
    Q_OBJECT
    using base_type = QComboBox;

public:
    QnIconSelectionComboBox(QWidget* parent = nullptr);
    virtual ~QnIconSelectionComboBox();

    void setIcons(const QString& path, const QStringList& names,
        const QString& extension = lit(".png"));

    int columnCount() const;
    void setColumnCount(int count);

    int maxRowCount() const;
    void setMaxRowCount(int count);

    QString currentIcon() const;
    void setCurrentIcon(const QString& name);

    virtual void showPopup() override;

protected:
    using base_type::currentText;
    using base_type::setCurrentText;

    virtual void adjustPopupParameters();

private:
    int m_columnCount = 0;
    int m_maxRowCount = 5;
};
