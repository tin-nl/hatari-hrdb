#ifndef DISASMWINDOW_H
#define DISASMWINDOW_H

#include <QDockWidget>
#include <QTableView>
#include "disassembler.h"

class TargetModel;
class Dispatcher;

class DisasmTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    DisasmTableModel(QObject * parent, TargetModel* pTargetModel);

    // "When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data()."
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    // "The model emits signals to indicate changes. For example, dataChanged() is emitted whenever items of data made available by the model are changed"
    // So I expect we can emit that if we see the target has changed

public slots:
    void memoryChangedSlot(int memorySlot);

private:
    TargetModel* m_pTargetModel;

    // These are taken at the same time. Is there a race condition...?
    Disassembler::disassembly m_disasm;
    uint32_t m_pc;
};

class DisasmWidget : public QDockWidget
{
    Q_OBJECT
public:
    DisasmWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* m_pDispatcher);
    void startStopChangedSlot();

private:
    QLineEdit*      m_pLineEdit;
    QTableView*     m_pTableView;

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
};

#endif // DISASMWINDOW_H
