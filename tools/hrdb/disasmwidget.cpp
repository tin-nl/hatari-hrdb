#include "disasmwidget.h"

#include <iostream>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QShortcut>
#include <QFontDatabase>

#include "dispatcher.h"
#include "targetmodel.h"


// ============================================================================
DisasmTableModel::DisasmTableModel(QObject *parent, TargetModel *pTargetModel, Dispatcher* pDispatcher) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher),
    m_memory(0, 0),
    m_addr(0),
    m_requestId(-1)
{
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &DisasmTableModel::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal, this, &DisasmTableModel::memoryChangedSlot);
    connect(m_pTargetModel, &TargetModel::breakpointsChangedSignal, this, &DisasmTableModel::breakpointsChangedSlot);
    connect(m_pTargetModel, &TargetModel::symbolTableChangedSignal, this, &DisasmTableModel::symbolTableChangedSlot);
}

int DisasmTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_disasm.lines.size();
}

int DisasmTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return kColCount;
}

QVariant DisasmTableModel::data(const QModelIndex &index, int role) const
{
    uint32_t row = (uint32_t)index.row();
    if (role == Qt::DisplayRole)
    {
        if (index.column() == kColSymbol)
        {
            uint32_t addr = m_disasm.lines[row].address;
            Symbol sym;
            if (m_pTargetModel->GetSymbolTable().Find(addr, sym))
                return QString::fromStdString(sym.name) + ":";
        }
        else if (index.column() == kColAddress)
        {
            uint32_t addr = m_disasm.lines[row].address;
            QString addrStr = QString::asprintf("%08x", addr);
            return addrStr;
        }
        else if (index.column() == kColBreakpoint)
        {
            uint32_t pc = m_pTargetModel->GetPC();
            QString bps;
            uint32_t addr = m_disasm.lines[row].address;
            for (size_t i = 0; i < m_breakpoints.m_breakpoints.size(); ++i)
            {
                if (m_breakpoints.m_breakpoints[i].m_pcHack == addr)
                    bps += "*";
            }

            if (pc == m_disasm.lines[row].address)
                bps += ">";
            return bps;
        }
        else if (index.column() == kColDisasm)
        {
            QString str;
            QTextStream ref(&str);
            Disassembler::print(m_disasm.lines[row].inst,
                    m_disasm.lines[row].address, ref);
            return str;
        }
        else if (index.column() == kColComments)
        {
            QString str;
            QTextStream ref(&str);
            Registers regs = m_pTargetModel->GetRegs();
            printEA(m_disasm.lines[row].inst.op0, regs, m_disasm.lines[row].address, ref);
            if (str.size() != 0)
                ref << "  ";
            printEA(m_disasm.lines[row].inst.op1, regs, m_disasm.lines[row].address, ref);
            return str;
        }
    }
    return QVariant(); // invalid item
}

QVariant DisasmTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
        case kColSymbol: return QString("Symbol");
        case kColAddress: return QString("Address");
        case kColBreakpoint: return QString("");    // Too narrow
        case kColDisasm: return QString("Disassembly");
        case kColComments: return QString("");
        }
    }
    return QVariant();
}

void DisasmTableModel::SetAddress(uint32_t addr)
{
    // Request memory for this region and save the address.
    m_requestId = m_pDispatcher->RequestMemory(MemorySlot::kDisasm, std::to_string(addr - 100), "300");
    m_addr = addr;
}

void DisasmTableModel::SetAddress(std::string addr)
{
    m_pDispatcher->RequestMemory(MemorySlot::kDisasm, addr, "100");
}

void DisasmTableModel::MoveUp()
{
    if (m_requestId != 0)
        return; // not up to date

    // TODO we should actually disassemble upwards to see if something sensible appears
    SetAddress(m_addr - 2);
}

void DisasmTableModel::MoveDown()
{
    if (m_requestId != 0)
        return; // not up to date

    if (m_disasm.lines.size() > 0)
    {
        // This will go off and request the memory itself
        SetAddress(m_disasm.lines[0].GetEnd());
    }
}

void DisasmTableModel::PageUp()
{
    if (m_requestId != 0)
        return; // not up to date

    // TODO we should actually disassemble upwards to see if something sensible appears
    SetAddress(m_addr - 20);
}

void DisasmTableModel::PageDown()
{
    if (m_requestId != 0)
        return; // not up to date

    if (m_disasm.lines.size() > 9)
    {
        // This will go off and request the memory itself
        SetAddress(m_disasm.lines[9].GetEnd());
    }
}

void DisasmTableModel::startStopChangedSlot()
{
    // Request new memory for the view
    if (!m_pTargetModel->IsRunning())
    {
        // Decide what to request.
        SetAddress(m_pTargetModel->GetPC());
    }
}

void DisasmTableModel::memoryChangedSlot(int memorySlot, uint64_t commandId)
{
    if (memorySlot != MemorySlot::kDisasm)
        return;

    // Only update for the last request we added
    if (commandId != m_requestId)
        return;

    const Memory* pMemOrig = m_pTargetModel->GetMemory(MemorySlot::kDisasm);
    if (!pMemOrig)
        return;

    // Cache it for later
    m_memory = *pMemOrig;

    // Make sure the data we get back matches our expectations...
    if (m_addr < m_memory.GetAddress())
        return;
    if (m_addr >= m_memory.GetAddress() + m_memory.GetSize())
        return;

    // Work out where we need to start disassembling from
    uint32_t offset = m_addr - m_memory.GetAddress();
    uint32_t size = m_memory.GetSize() - offset;
    buffer_reader disasmBuf(m_memory.GetData() + offset, size);
    m_disasm.lines.clear();
    Disassembler::decode_buf(disasmBuf, m_disasm, m_addr, 10);

    // Clear the request, to say we are up to date
    m_requestId = 0;

    emit beginResetModel();
    emit endResetModel();
}

void DisasmTableModel::breakpointsChangedSlot(uint64_t commandId)
{
    // Cache data
    m_breakpoints = m_pTargetModel->GetBreakpoints();
    emit beginResetModel();
    emit endResetModel();
}

void DisasmTableModel::symbolTableChangedSlot(uint64_t commandId)
{
    // Don't copy here, just force a re-read
    emit beginResetModel();
    emit endResetModel();
}

void DisasmTableModel::ToggleBreakpoint(const QModelIndex& index)
{
    // set a breakpoint
    uint32_t addr = m_disasm.lines[index.row()].address;
    bool removed = false;

    const Breakpoints& bp = m_pTargetModel->GetBreakpoints();
    for (size_t i = 0; i < bp.m_breakpoints.size(); ++i)
    {
        if (bp.m_breakpoints[i].m_pcHack == addr)
        {
            QString cmd = QString::asprintf("bpdel %d", i + 1);
            m_pDispatcher->SendCommandPacket(cmd.toStdString().c_str());
            removed = true;
        }
    }
    if (!removed)
    {
        QString cmd = QString::asprintf("bp pc = %d", addr);
        m_pDispatcher->SendCommandPacket(cmd.toStdString().c_str());
    }
    m_pDispatcher->SendCommandPacket("bplist");
}

void DisasmTableModel::printEA(const operand& op, const Registers& regs, uint32_t address, QTextStream& ref) const
{
    uint32_t ea;
    if (Disassembler::calc_fixed_ea(op, regs, address, ea))
    {
        ea &= 0xffffff; // mask for ST hardware range
        ref << "$" << QString::asprintf("%x", ea);
        Symbol sym;
        if (m_pTargetModel->GetSymbolTable().FindLowerOrEqual(ea, sym))
        {
            uint32_t offset = ea - sym.address;
            if (offset)
                ref << " " << QString::fromStdString(sym.name) << "+" << offset;
            else
                ref << " " << QString::fromStdString(sym.name);
        }
    };
}



DisasmWidget::DisasmWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDockWidget(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    this->setWindowTitle("Disassembly");
    QVBoxLayout *layout = new QVBoxLayout;
    auto pGroupBox = new QGroupBox(this);

    m_pLineEdit = new QLineEdit(this);
    m_pTableView = new QTableView(this);

    m_pTableModel = new DisasmTableModel(this, pTargetModel, pDispatcher);
    m_pTableView->setModel(m_pTableModel);

    m_pTableView->horizontalHeader()->setMinimumSectionSize(12);
    //m_pTableView->horizontalHeader()->hide();
    m_pTableView->setColumnWidth(DisasmTableModel::kColSymbol, 10*15);
    m_pTableView->setColumnWidth(DisasmTableModel::kColAddress, 10*8);      // Windows needs more
    m_pTableView->setColumnWidth(DisasmTableModel::kColBreakpoint, 32);
    m_pTableView->setColumnWidth(DisasmTableModel::kColDisasm, 300);
    m_pTableView->setColumnWidth(DisasmTableModel::kColComments, 300);

    m_pTableView->verticalHeader()->hide();
    //m_pTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    m_pTableView->verticalHeader()->setDefaultSectionSize(20);// affects Linux but not Windows
    // These have no effect
    //m_pTableView->verticalHeader()->contentsMargins().setTop(0);
    //m_pTableView->verticalHeader()->contentsMargins().setBottom(0);
    m_pTableView->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
    m_pTableView->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);

    layout->addWidget(m_pLineEdit);
    layout->addWidget(m_pTableView);
    pGroupBox->setLayout(layout);
    setWidget(pGroupBox);

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_pTableView->setFont(monoFont);
    m_pTableView->resizeRowsToContents();

    // Listen for start/stop, so we can update our memory request
    connect(m_pTableView,   &QTableView::clicked,                 this, &DisasmWidget::cellClickedSlot);
    connect(m_pLineEdit, &QLineEdit::textChanged,                 this, &DisasmWidget::textEditChangedSlot);

    new QShortcut(QKeySequence(tr("Down", "Next instructions")), this, SLOT(keyDownPressed()));
    new QShortcut(QKeySequence(tr("Up",   "Prev instructions")), this, SLOT(keyUpPressed()));
    new QShortcut(QKeySequence(QKeySequence::MoveToNextPage),     this, SLOT(keyPageDownPressed()));
    new QShortcut(QKeySequence(QKeySequence::MoveToPreviousPage), this, SLOT(keyPageUpPressed()));
}

void DisasmWidget::cellClickedSlot(const QModelIndex &index)
{
    if (index.column() != DisasmTableModel::kColBreakpoint)
        return;
    m_pTableModel->ToggleBreakpoint(index);
}

void DisasmWidget::keyDownPressed()
{
    m_pTableModel->MoveDown();
}
void DisasmWidget::keyUpPressed()
{
    m_pTableModel->MoveUp();
}

void DisasmWidget::keyPageDownPressed()
{
    m_pTableModel->PageDown();
}
void DisasmWidget::keyPageUpPressed()
{
    m_pTableModel->PageUp();
}

void DisasmWidget::textEditChangedSlot()
{
    m_pTableModel->SetAddress(m_pLineEdit->text().toStdString());
}

