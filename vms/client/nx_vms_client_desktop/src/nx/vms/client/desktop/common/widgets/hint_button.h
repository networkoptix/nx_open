// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QAbstractButton>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/color_theme.h>

class QGroupBox;

namespace nx::vms::client::desktop {

/**
 * Question mark that spawns tooltip when hovered.
 * It can be attached to QGroupBox and appear right after its caption.
 * It gets ID of help topic using utilities from help_topic_accessor.h
 */
class HintButton: public QAbstractButton
{
    using base_type = QAbstractButton;
    Q_OBJECT;
    Q_PROPERTY(QString hintText READ hintText WRITE setHintText)

public:
    HintButton(QWidget* parent = nullptr);
    HintButton(const QString& hintText, QWidget* parent = nullptr);

    /**
     * Creates hint button attached to the header of the group box, ownership of the created hint
     * is transferred to the group box.
     */
    static HintButton* createGroupBoxHint(QGroupBox* groupBox);

    /**
     * @returns Text displayed in the tooltip.
     */
    QString hintText() const;

    /**
     * Sets text which will be displayed in the tooltip. Each line of text will appear as paragraph.
     */
    void setHintText(const QString& hintText);

    /**
     * Appends line to the end of the existing hint text.
     */
    void addHintLine(const QString& hintLine);

    static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconSubstitutions;

protected:
    virtual bool event(QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void enterEvent(QEnterEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void showTooltip(bool show);
    void updateGeometry(QGroupBox* parent); //< Updates position of hint attached to a group box.
    int getHelpTopicId() const;
    bool hasHelpTopic() const;

private:
    QPixmap m_regularPixmap;
    QPixmap m_highlightedPixmap;
    QString m_hintText;
    QPointer<QGroupBox> m_groupBox; //< Pointer to the group box to which the hint is attached.

    bool m_isHovered = false;
    bool m_tooltipVisible = false;
};

} // namespace nx::vms::client::desktop
