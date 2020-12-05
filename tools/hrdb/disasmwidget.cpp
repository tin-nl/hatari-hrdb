#include "disasmwidget.h"

#include <iostream>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QStringListModel>

#include "dispatcher.h"
#include "targetmodel.h"

DisasmWidget::DisasmWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDockWidget(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    QVBoxLayout *layout = new QVBoxLayout;
    auto pGroupBox = new QGroupBox(this);

    m_pLineEdit = new QLineEdit(this);
    m_pTableView = new QTableView(this);

    m_pTableModel = new DisasmTableModel(this, pTargetModel);
    m_pTableView->setModel(m_pTableModel);

    m_pTableView->horizontalHeader()->setMinimumSectionSize(0);
    m_pTableView->horizontalHeader()->hide();
    m_pTableView->setColumnWidth(0, 9*8);
    m_pTableView->setColumnWidth(1, 16);
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
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &DisasmWidget::startStopChangedSlot);
    connect(m_pTableView,   &QTableView::clicked,                 this, &DisasmWidget::cellClickedSlot);
}

void DisasmWidget::startStopChangedSlot()
{
    // Request new memory for the view
    if (!m_pTargetModel->IsRunning())
    {
        m_pDispatcher->RequestMemory(MemorySlot::kDisasm, "pc", "100");
    }
}

// ============================================================================
DisasmTableModel::DisasmTableModel(QObject *parent, TargetModel *pTargetModel) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_pc(0)
{
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
            QString bps;
            uint32_t addr = m_disasm.lines[row].address;
            for (size_t i = 0; i < m_breakpoints.m_breakpoints.size(); ++i)
            {
                if (m_breakpoints.m_breakpoints[i].m_pcHack == addr)
                    bps += "*";
            }

            if (m_pc == m_disasm.lines[row].address)
                bps += ">>";
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

void DisasmTableModel::memoryChangedSlot(int memorySlot)
{
    if (memorySlot != MemorySlot::kDisasm)
        return;

    m_disasm.lines.clear();
    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kDisasm);
    if (!pMem)
        return;

    // Fetch underlying data, this is picked up by the model class
    buffer_reader disasmBuf(pMem->GetData(), pMem->GetSize());
    Disassembler::decode_buf(disasmBuf, m_disasm, pMem->GetAddress(), 10);
    m_pc = m_pTargetModel->GetPC();

    emit beginResetModel();
    emit endResetModel();
}

void DisasmTableModel::breakpointsChangedSlot()
{
    m_breakpoints = m_pTargetModel->GetBreakpoints();
    emit beginResetModel();
    emit endResetModel();
}

void DisasmWidget::cellClickedSlot(const QModelIndex &index)
{
    if (index.column() != 1)
        return;

    // set a breakpoint
    uint32_t addr = m_pTableModel->m_disasm.lines[index.row()].address;
    QString cmd = QString::asprintf("bp %d", addr);
    m_pDispatcher->SendCommandPacket(cmd.toStdString().c_str());
    m_pDispatcher->SendCommandPacket("bplist");
}

