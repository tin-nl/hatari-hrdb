#ifndef DISASMWINDOW_H
#define DISASMWINDOW_H

#include <QDockWidget>

class DisasmWidget : public QDockWidget
{
    Q_OBJECT
public:
    explicit DisasmWidget(QWidget *parent = nullptr);
};

#endif // DISASMWINDOW_H
