#ifndef DISASMWINDOW_H
#define DISASMWINDOW_H

#include <QDockWidget>
#include <QTableView>
#include "disassembler.h"
#include "breakpoint.h"
#include "memory.h"

class TargetModel;
class Dispatcher;

class DisasmTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column
    {
        kColSymbol,
        kColAddress,
        kColBreakpoint,
        kColDisasm,
        kColComments,

        kColCount
    };

    DisasmTableModel(QObject * parent, TargetModel* pTargetModel, Dispatcher* m_pDispatcher);

    // "When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data()."
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    // "The model emits signals to indicate changes. For example, dataChanged() is emitted whenever items of data made available by the model are changed"
    // So I expect we can emit that if we see the target has changed

    uint32_t GetAddress() const { return m_addr; }
    void SetAddress(uint32_t addr);
    void SetAddress(std::string addr);
    void MoveUp();
    void MoveDown();
    void PageUp();
    void PageDown();
    void ToggleBreakpoint(const QModelIndex &index);

public slots:
    void startStopChangedSlot();
    void memoryChangedSlot(int memorySlot, uint64_t commandId);
    void breakpointsChangedSlot(uint64_t commandId);
    void symbolTableChangedSlot(uint64_t commandId);

private:
    void printEA(const operand &op, const Registers &regs, uint32_t address, QTextStream &ref) const;
    TargetModel* m_pTargetModel;
    Dispatcher*  m_pDispatcher;

    // Cached data when the up-to-date request comes through
    Memory       m_memory;
    Disassembler::disassembly m_disasm;
    Breakpoints m_breakpoints;

    // Address of the top line of text that was requested
    uint32_t m_addr;            // Most recent address request
    uint64_t m_requestId;       // Most recent memory request
};

class DisasmWidget : public QDockWidget
{
    Q_OBJECT
public:
    DisasmWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* m_pDispatcher);

protected:

protected slots:
    void cellClickedSlot(const QModelIndex& index);
    void keyDownPressed();
    void keyUpPressed();
    void keyPageDownPressed();
    void keyPageUpPressed();
    void textEditChangedSlot();

private:
    QLineEdit*      m_pLineEdit;
    QTableView*     m_pTableView;
    DisasmTableModel* m_pTableModel;

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
};

#endif // DISASMWINDOW_H
