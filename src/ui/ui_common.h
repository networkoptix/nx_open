#ifndef ui_common_h_1340
#define ui_common_h_1340

class QWidget;

enum ViewMode{UNDEFINED_ViewMode, NORMAL_ViewMode, LAYOUTEDITOR_ViewMode};

QString UIgetText(QWidget* parent, const QString& title, const QString& labletext, const QString& deftext, bool& ok);
void UIOKMessage(QWidget* parent, const QString& title, const QString& text);
QMessageBox::StandardButton YesNoCancel(QWidget *parent, const QString &title, const QString& text);

QString UIDisplayName(const QString& fullname);

#endif
