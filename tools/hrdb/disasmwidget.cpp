#include "disasmwidget.h"

#include <iostream>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QShortcut>
#include <QFontDatabase>
#include <QMenu>
#include <QContextMenuEvent>
#include <QCheckBox>

#include "dispatcher.h"
#include "targetmodel.h"
#include "stringparsers.h"

//-----------------------------------------------------------------------------
DisasmTableModel::DisasmTableModel(QObject *parent, TargetModel *pTargetModel, Dispatcher* pDispatcher) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher),
    m_memory(0, 0),
    m_rowCount(25),
    m_requestedAddress(0),
    m_logicalAddr(0),
    m_requestId(0),
    m_bFollowPC(true)
{
    m_breakpoint10Pixmap = QPixmap(":/images/breakpoint10.png");

    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &DisasmTableModel::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal, this, &DisasmTableModel::memoryChangedSlot);
    connect(m_pTargetModel, &TargetModel::breakpointsChangedSignal, this, &DisasmTableModel::breakpointsChangedSlot);
    connect(m_pTargetModel, &TargetModel::symbolTableChangedSignal, this, &DisasmTableModel::symbolTableChangedSlot);
}

int DisasmTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    // Find how many pixels a row takes
    return std::max(m_rowCount, 1);
    //return m_disasm.lines.size();
}

int DisasmTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return kColCount;
}

QVariant DisasmTableModel::data(const QModelIndex &index, int role) const
{
    // Always empty if we have no disassembly ready
    uint32_t row = (uint32_t)index.row();
    if (row >= m_disasm.lines.size())
        return QVariant();

    const Disassembler::line& line = m_disasm.lines[row];
    if (role == Qt::DisplayRole)
    {
        if (index.column() == kColSymbol)
        {
            uint32_t addr = line.address;
            Symbol sym;
            if (m_pTargetModel->GetSymbolTable().Find(addr, sym))
                return QString::fromStdString(sym.name) + ":";
        }
        else if (index.column() == kColAddress)
        {
            uint32_t addr = line.address;
            QString addrStr = QString::asprintf("%08x", addr);
            return addrStr;
        }
        else if (index.column() == kColBreakpoint)
        {
            uint32_t pc = m_pTargetModel->GetPC();
            if (pc == line.address)
                return QString(">");
        }
        else if (index.column() == kColDisasm)
        {
            QString str;
            QTextStream ref(&str);
            Disassembler::print(line.inst,
                    line.address, ref);
            return str;
        }
        else if (index.column() == kColComments)
        {
            QString str;
            QTextStream ref(&str);
            Registers regs = m_pTargetModel->GetRegs();
            printEA(line.inst.op0, regs, line.address, ref);
            if (str.size() != 0)
                ref << "  ";
            printEA(line.inst.op1, regs, line.address, ref);
            return str;
        }
    }
    else if (role == Qt::DecorationRole)
    {
        if (row >= m_disasm.lines.size())
            return QVariant();

        if (index.column() == kColBreakpoint)
        {
            uint32_t addr = line.address;
            for (size_t i = 0; i < m_breakpoints.m_breakpoints.size(); ++i)
            {
                if (m_breakpoints.m_breakpoints[i].m_pcHack == addr)
                {
                    return m_breakpoint10Pixmap;
                }
            }
        }
    }
    else if (role == Qt::BackgroundColorRole)
    {
        // Highlight the PC line
        if (!m_pTargetModel->IsRunning())
        {
            if (line.address == m_pTargetModel->GetPC())
                return QVariant(QColor(Qt::yellow));
        }
    }
    return QVariant(); // invalid item
}

QVariant DisasmTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Orientation::Horizontal)
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
        if (role == Qt::TextAlignmentRole)
        {
            return Qt::AlignLeft;
        }
    }
    return QVariant();
}

void DisasmTableModel::SetAddress(uint32_t addr)
{
    // Request memory for this region and save the address.
    m_logicalAddr = addr;
    RequestMemory();
    emit addressChanged(m_logicalAddr);
}

// Request enough memory based on m_rowCount and m_logicalAddr
void DisasmTableModel::RequestMemory()
{
    uint32_t addr = m_logicalAddr;
    uint32_t lowAddr = (addr > 100) ? addr - 100 : 0;
    uint32_t size = ((m_rowCount * 10) + 100);
    m_requestId = m_pDispatcher->RequestMemory(MemorySlot::kDisasm, lowAddr, size);
}

bool DisasmTableModel::SetAddress(std::string addrStr)
{
    uint32_t addr;
    if (!StringParsers::ParseExpression(addrStr.c_str(), addr,
                                       m_pTargetModel->GetSymbolTable(), m_pTargetModel->GetRegs()))
    {
        return false;
    }

    SetAddress(addr);
    return true;
}

void DisasmTableModel::MoveUp()
{
    if (m_requestId != 0)
        return; // not up to date

    // Disassemble upwards to see if something sensible appears.
    // Stop at the first valid instruction opcode.
    // Max instruction size is 10 bytes.
    if (m_requestId == 0)
    {
        for (uint32_t off = 2; off <= 10; off += 2)
        {
            uint32_t targetAddr = m_logicalAddr - off;
            // Check valid memory
            if (m_memory.GetAddress() > targetAddr ||
                m_memory.GetAddress() + m_memory.GetSize() <= targetAddr)
            {
                continue;
            }
            // Get memory buffer for this range
            uint32_t offset = targetAddr - m_memory.GetAddress();
            uint32_t size = m_memory.GetSize() - offset;
            buffer_reader disasmBuf(m_memory.GetData() + offset, size);
            instruction inst;
            if (Disassembler::decode_inst(disasmBuf, inst) == 0)
            {
                if (inst.opcode != Opcode::NONE)
                {
                    SetAddress(targetAddr);
                    return;
                }
            }
        }
    }

    if (m_logicalAddr > 2)
        SetAddress(m_logicalAddr - 2);
    else
        SetAddress(0);
}

void DisasmTableModel::MoveDown()
{
    if (m_requestId != 0)
        return; // not up to date

    if (m_disasm.lines.size() > 0)
    {
        // Find our current line in disassembly
        for (size_t i = 0; i < m_disasm.lines.size(); ++i)
        {
            if (m_disasm.lines[i].address == m_logicalAddr)
            {
                // This will go off and request the memory itself
                SetAddress(m_disasm.lines[i].GetEnd());
                return;
            }
        }
        // Default to line 1
        SetAddress(m_disasm.lines[0].GetEnd());
    }
}

void DisasmTableModel::PageUp()
{
    if (m_requestId != 0)
        return; // not up to date

    // TODO we should actually disassemble upwards to see if something sensible appears
    if (m_logicalAddr > 20)
        SetAddress(m_logicalAddr - 20);
    else
        SetAddress(0);
}

void DisasmTableModel::PageDown()
{
    if (m_requestId != 0)
        return; // not up to date

    if (m_disasm.lines.size() > 0)
    {
        // This will go off and request the memory itself
        SetAddress(m_disasm.lines.back().GetEnd());
    }
}

void DisasmTableModel::RunToRow(int row)
{
    if (row >= 0 && row < m_disasm.lines.size())
    {
        Disassembler::line& line = m_disasm.lines[row];
        m_pDispatcher->RunToPC(line.address);
    }
}

void DisasmTableModel::startStopChangedSlot()
{
    // Request new memory for the view
    if (!m_pTargetModel->IsRunning())
    {
        // Decide what to request.
        if (m_bFollowPC)
        {
            // Update to PC position
            SetAddress(m_pTargetModel->GetPC());
        }
        else
        {
            // Just request what we had already.
            RequestMemory();
        }
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

    if (m_logicalAddr == kInvalid)
    {
        m_logicalAddr = pMemOrig->GetAddress();
        emit addressChanged(m_logicalAddr);
    }

    // Cache it for later
    m_memory = *pMemOrig;
    CalcDisasm();

    // Clear the request, to say we are up to date
    m_requestId = 0;
    emit dataChanged(this->createIndex(0, 0), this->createIndex(m_rowCount - 1, kColCount));
}

void DisasmTableModel::breakpointsChangedSlot(uint64_t commandId)
{
    // Cache data
    m_breakpoints = m_pTargetModel->GetBreakpoints();
    emit dataChanged(this->createIndex(0, 0), this->createIndex(m_rowCount - 1, kColCount));
}

void DisasmTableModel::symbolTableChangedSlot(uint64_t commandId)
{
    // Don't copy here, just force a re-read
    emit dataChanged(this->createIndex(0, 0), this->createIndex(m_rowCount - 1, kColCount));
}

void DisasmTableModel::CalcDisasm()
{
    // Make sure the data we get back matches our expectations...
    if (m_logicalAddr < m_memory.GetAddress())
        return;
    if (m_logicalAddr >= m_memory.GetAddress() + m_memory.GetSize())
        return;

    // Work out where we need to start disassembling from
    uint32_t offset = m_logicalAddr - m_memory.GetAddress();
    uint32_t size = m_memory.GetSize() - offset;
    buffer_reader disasmBuf(m_memory.GetData() + offset, size);
    m_disasm.lines.clear();
    Disassembler::decode_buf(disasmBuf, m_disasm, m_logicalAddr, m_rowCount);
}

void DisasmTableModel::ToggleBreakpoint(int row)
{
    // set a breakpoint
    if (row >= m_disasm.lines.size())
        return;

    Disassembler::line& line = m_disasm.lines[row];
    uint32_t addr = line.address;
    bool removed = false;

    const Breakpoints& bp = m_pTargetModel->GetBreakpoints();
    for (size_t i = 0; i < bp.m_breakpoints.size(); ++i)
    {
        if (bp.m_breakpoints[i].m_pcHack == addr)
        {
            QString cmd = QString::asprintf("bpdel %d", i + 1);
            m_pDispatcher->SendCommandPacket(cmd.toStdString().c_str());
            removed = true;
            m_pDispatcher->SendCommandPacket("bplist");
        }
    }
    if (!removed)
    {
        QString cmd = QString::asprintf("pc = %d", addr);
        m_pDispatcher->SetBreakpoint(cmd.toStdString().c_str());
    }
}

void DisasmTableModel::SetRowCount(int count)
{
    if (count != m_rowCount)
    {
        emit beginResetModel();
        m_rowCount = count;

        // Do we need more data?
        CalcDisasm();
        if (m_disasm.lines.size() < m_rowCount)
        {
            // We need more memory
            RequestMemory();
        }

        emit endResetModel();
    }
}

void DisasmTableModel::SetFollowPC(bool bFollow)
{
    m_bFollowPC = bFollow;
}


void DisasmTableModel::printEA(const operand& op, const Registers& regs, uint32_t address, QTextStream& ref) const
{
    uint32_t ea;

    // Only do a full analysis if this is the PC and therefore the registers are valid...
    if (Disassembler::calc_fixed_ea(op, address == m_pTargetModel->GetPC(), regs, address, ea))
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

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DisasmTableView::DisasmTableView(QWidget* parent, DisasmTableModel* pModel, TargetModel* pTargetModel) :
    QTableView(parent),
    m_pTableModel(pModel),
    m_rightClickMenu(this),
    m_rightClickRow(-1)
{
    // Actions for right-click menu
    m_pRunUntilAction = new QAction(tr("Run to here"), this);
    connect(m_pRunUntilAction, &QAction::triggered, this, &DisasmTableView::runToCursorRightClick);
    m_pBreakpointAction = new QAction(tr("Toggle Breakpoint"), this);
    m_rightClickMenu.addAction(m_pRunUntilAction);
    m_rightClickMenu.addAction(m_pBreakpointAction);

    new QShortcut(QKeySequence(tr("F3", "Run to cursor")),        this, SLOT(runToCursor()));
    new QShortcut(QKeySequence(tr("F9", "Toggle breakpoint")),    this, SLOT(toggleBreakpoint()));

    connect(m_pBreakpointAction, &QAction::triggered,                  this, &DisasmTableView::toggleBreakpointRightClick);
    connect(pTargetModel,        &TargetModel::startStopChangedSignal, this, &DisasmTableView::RecalcRowCount);

    // This table gets the focus from the parent docking widget
    setFocus();
}

void DisasmTableView::contextMenuEvent(QContextMenuEvent *event)
{
    QModelIndex index = this->indexAt(event->pos());
    if (!index.isValid())
        return;

    m_rightClickRow = index.row();
    m_rightClickMenu.exec(event->globalPos());

}

void DisasmTableView::runToCursorRightClick()
{
    m_pTableModel->RunToRow(m_rightClickRow);
    m_rightClickRow = -1;
}

void DisasmTableView::toggleBreakpointRightClick()
{
    m_pTableModel->ToggleBreakpoint(m_rightClickRow);
    m_rightClickRow = -1;
}

void DisasmTableView::runToCursor()
{
    // How do we get the selected row
    QModelIndex i = this->currentIndex();
    m_pTableModel->RunToRow(i.row());
}

void DisasmTableView::toggleBreakpoint()
{
    // How do we get the selected row
    QModelIndex i = this->currentIndex();
    m_pTableModel->ToggleBreakpoint(i.row());
}

QModelIndex DisasmTableView::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    QModelIndex i = this->currentIndex();

    // Do the override/refill behaviour if we need to scroll our virtual area
    if (cursorAction == QAbstractItemView::CursorAction::MoveUp &&
        i.row() == 0)
    {
        m_pTableModel->MoveUp();
        return i;
    }
    else if (cursorAction == QAbstractItemView::CursorAction::MoveDown &&
             i.row() >= m_pTableModel->GetRowCount() - 1)
    {
        m_pTableModel->MoveDown();
        return i;
    }
    else if (cursorAction == QAbstractItemView::CursorAction::MovePageUp &&
             i.row() == 0)
    {
        m_pTableModel->PageUp();
        return i;
    }
    else if (cursorAction == QAbstractItemView::CursorAction::MovePageDown &&
             i.row() >= m_pTableModel->GetRowCount() - 1)
    {
        m_pTableModel->PageDown();
        return i;
    }

    return QTableView::moveCursor(cursorAction, modifiers);
}

void DisasmTableView::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);
    RecalcRowCount();
}

void DisasmTableView::RecalcRowCount()
{
    // It seems that viewport is updated without this even being called,
    // which means that on startup, "h" == 0.
    int h = this->viewport()->size().height();
    int rowh = this->rowHeight(0);
    if (rowh != 0)
        m_pTableModel->SetRowCount(h / rowh);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

DisasmWidget::DisasmWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDockWidget(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    // Create model early
    m_pTableModel = new DisasmTableModel(this, pTargetModel, pDispatcher);

    this->setWindowTitle("Disassembly");
    QVBoxLayout *layout = new QVBoxLayout;
    auto pGroupBox = new QGroupBox(this);

    m_pLineEdit = new QLineEdit(this);

    m_pTableView = new DisasmTableView(this, m_pTableModel, m_pTargetModel);
    m_pTableView->setModel(m_pTableModel);

    m_pFollowPC = new QCheckBox("Follow PC", this);
    m_pFollowPC->setTristate(false);
    m_pFollowPC->setChecked(m_pTableModel->GetFollowPC());

    //QWidget* pTempWidget = new QTableSc(this);
    //pTempWidget->setEnabled(true);
    //m_pTableView->setViewport(pTempWidget);

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_pTableView->setFont(monoFont);
    QFontMetrics fm(monoFont);

    // This is across the top
    int charWidth = fm.width("W");
    m_pTableView->horizontalHeader()->setMinimumSectionSize(12);
    m_pTableView->setColumnWidth(DisasmTableModel::kColSymbol, charWidth * 15);
    m_pTableView->setColumnWidth(DisasmTableModel::kColAddress, charWidth * 9);      // Windows needs more
    m_pTableView->setColumnWidth(DisasmTableModel::kColBreakpoint, charWidth * 4);
    m_pTableView->setColumnWidth(DisasmTableModel::kColDisasm, charWidth * 30);
    m_pTableView->setColumnWidth(DisasmTableModel::kColComments, 300);

    // Down the side
    m_pTableView->verticalHeader()->hide();
    m_pTableView->verticalHeader()->setDefaultSectionSize(fm.height());

    // We don't allow selection. The active key always happens on row 0
    m_pTableView->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
    m_pTableView->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
    m_pTableView->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    layout->addWidget(m_pLineEdit);
    layout->addWidget(m_pFollowPC);
    layout->addWidget(m_pTableView);
    pGroupBox->setLayout(layout);
    setWidget(pGroupBox);

    // Listen for start/stop, so we can update our memory request
    connect(m_pTableView,   &QTableView::clicked,                 this, &DisasmWidget::cellClickedSlot);
    connect(m_pTableModel,  &DisasmTableModel::addressChanged,    this, &DisasmWidget::UpdateTextBox);
    connect(m_pLineEdit,    &QLineEdit::returnPressed,            this, &DisasmWidget::returnPressedSlot);
    connect(m_pLineEdit,    &QLineEdit::textEdited,               this, &DisasmWidget::textChangedSlot);
    connect(m_pFollowPC,    &QCheckBox::clicked,                  this, &DisasmWidget::followPCClickedSlot);

    this->resizeEvent(nullptr);
}

void DisasmWidget::cellClickedSlot(const QModelIndex &index)
{
    if (index.column() != DisasmTableModel::kColBreakpoint)
        return;
    m_pTableModel->ToggleBreakpoint(index.row());
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

void DisasmWidget::returnPressedSlot()
{
    QColor col = m_pTableModel->SetAddress(m_pLineEdit->text().toStdString()) ?
                      Qt::white : Qt::red;
    QPalette pal = m_pLineEdit->palette();
    pal.setColor(QPalette::Base, col);
    m_pLineEdit-> setAutoFillBackground(true);
    m_pLineEdit->setPalette(pal);
}

void DisasmWidget::textChangedSlot()
{
    uint32_t addr;
    QColor col = Qt::green;
    if (!StringParsers::ParseExpression(m_pLineEdit->text().toStdString().c_str(), addr,
                                       m_pTargetModel->GetSymbolTable(), m_pTargetModel->GetRegs()))
    {
        col = Qt::red;
    }

    QPalette pal = m_pLineEdit->palette();
    pal.setColor(QPalette::Base, col);
    m_pLineEdit-> setAutoFillBackground(true);
    m_pLineEdit->setPalette(pal);
}

void DisasmWidget::followPCClickedSlot()
{
    m_pTableModel->SetFollowPC(m_pFollowPC->isChecked());
}

void DisasmWidget::UpdateTextBox()
{
    uint32_t addr = m_pTableModel->GetAddress();
    m_pLineEdit->setText(QString::asprintf("$%x", addr));
}

