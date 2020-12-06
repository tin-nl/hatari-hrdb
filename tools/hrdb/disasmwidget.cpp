#include "disasmwidget.h"

#include <iostream>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QShortcut>

#include "dispatcher.h"
#include "targetmodel.h"


// ============================================================================
DisasmTableModel::DisasmTableModel(QObject *parent, TargetModel *pTargetModel, Dispatcher* pDispatcher) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher),
    m_memory(0, 0),
    m_addr(0)
{
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &DisasmTableModel::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal, this, &DisasmTableModel::memoryChangedSlot);
    connect(m_pTargetModel, &TargetModel::breakpointsChangedSignal, this, &DisasmTableModel::breakpointsChangedSlot);
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

    return 3;
}

QVariant DisasmTableModel::data(const QModelIndex &index, int role) const
{
    uint32_t row = (uint32_t)index.row();
    if (role == Qt::DisplayRole)
    {
        if (index.column() == 0)
        {
            QString addr = QString::asprintf("%08x", m_disasm.lines[row].address);
            return addr;
        }
        else if (index.column() == 1)
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
        else if (index.column() == 2)
        {
            QString str;
            QTextStream ref(&str);
            Disassembler::print(m_disasm.lines[row].inst,
                    m_disasm.lines[row].address, ref);
            return str;
        }
        return QString("addr");
    }
    return QVariant(); // invalid item
}

void DisasmTableModel::SetAddress(uint32_t addr)
{
    // Request memory
    uint32_t topAddr = addr;
    if (addr > 60)
        topAddr = addr - 60;
    uint32_t size = 60 + 100 + 60;
    m_pDispatcher->RequestMemory(MemorySlot::kDisasm, std::to_string(topAddr), std::to_string(size));
    m_addr = addr;
}

void DisasmTableModel::MoveUp()
{
    // TODO we should actually disassemble upwards to see if something sensible appears
    SetAddress(m_addr - 2);
}

void DisasmTableModel::MoveDown()
{
    if (m_disasm.lines.size() > 0)
    {
        // This will go off and request the memory itself
        SetAddress(m_disasm.lines[0].GetEnd());
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

void DisasmTableModel::memoryChangedSlot(int memorySlot)
{
    if (memorySlot != MemorySlot::kDisasm)
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

    emit beginResetModel();
    emit endResetModel();
}

void DisasmTableModel::breakpointsChangedSlot()
{
    // Cache data
    m_breakpoints = m_pTargetModel->GetBreakpoints();
    emit beginResetModel();
    emit endResetModel();
}

DisasmWidget::DisasmWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDockWidget(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    QVBoxLayout *layout = new QVBoxLayout;
    auto pGroupBox = new QGroupBox(this);

    m_pLineEdit = new QLineEdit(this);
    m_pTableView = new QTableView(this);

    m_pTableModel = new DisasmTableModel(this, pTargetModel, pDispatcher);
    m_pTableView->setModel(m_pTableModel);

    m_pTableView->horizontalHeader()->setMinimumSectionSize(0);
    m_pTableView->horizontalHeader()->hide();
    m_pTableView->setColumnWidth(0, 9*8);
    m_pTableView->setColumnWidth(1, 32);
    m_pTableView->setColumnWidth(2, 300);

    m_pTableView->verticalHeader()->hide();
    //m_pTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_pTableView->verticalHeader()->setDefaultSectionSize(16);

    layout->addWidget(m_pLineEdit);
    layout->addWidget(m_pTableView);
    pGroupBox->setLayout(layout);
    setWidget(pGroupBox);

    QFont monoFont("Monospace");
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(9);
    m_pTableView->setFont(monoFont);
    m_pTableView->resizeRowsToContents();

    // Listen for start/stop, so we can update our memory request
    connect(m_pTableView,   &QTableView::clicked,                 this, &DisasmWidget::cellClickedSlot);

    new QShortcut(QKeySequence(tr("Down", "Next instructions")),
                    this,
                    SLOT(keyDownPressed()));
    new QShortcut(QKeySequence(tr("Up", "Prev instructions")),
                    this,
                    SLOT(keyUpPressed()));
}

void DisasmWidget::cellClickedSlot(const QModelIndex &index)
{
    if (index.column() != 1)
        return;

    // set a breakpoint
    uint32_t addr = m_pTableModel->m_disasm.lines[index.row()].address;
    bool removed = false;

    for (size_t i = 0; i < m_pTableModel->m_breakpoints.m_breakpoints.size(); ++i)
    {
        if (m_pTableModel->m_breakpoints.m_breakpoints[i].m_pcHack == addr)
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

void DisasmWidget::keyDownPressed()
{
    m_pTableModel->MoveDown();
}
void DisasmWidget::keyUpPressed()
{
    m_pTableModel->MoveUp();
}


