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

    DisasmTableModel* pModel = new DisasmTableModel(this, pTargetModel);
    m_pTableView->setModel(pModel);

    m_pTableView->horizontalHeader()->setMinimumSectionSize(0);
    m_pTableView->horizontalHeader()->hide();
    m_pTableView->setColumnWidth(0, 9*8);
    m_pTableView->setColumnWidth(1, 16);

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
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal,   this, &DisasmWidget::startStopChangedSlot);
}

void DisasmWidget::startStopChangedSlot()
{
    // Request new memory for the view
    if (!m_pTargetModel->IsRunning())
    {
        uint32_t pc = m_pTargetModel->GetPC();
        m_pDispatcher->RequestMemory(MemorySlot::kDisasm, pc, 100);
    }
}

DisasmTableModel::DisasmTableModel(QObject *parent, TargetModel *pTargetModel) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_pc(0)
{
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal, this, &DisasmTableModel::memoryChangedSlot);
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
    if (role == Qt::DisplayRole)
    {
        if (index.column() == 0)
        {
            QString addr = QString::asprintf("%08x", m_disasm.lines[index.row()].address);
            return addr;
        }
        else if (index.column() == 1)
        {
            if (m_pc == m_disasm.lines[index.row()].address)
            {
                return QString(">");
            }
            return QVariant(); // invalid item
        }
        else if (index.column() == 2)
        {
            QString str;
            QTextStream ref(&str);
            Disassembler::print(m_disasm.lines[index.row()].inst,
                    m_disasm.lines[index.row()].address, ref);
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

    // Fetch underlying data
    buffer_reader disasmBuf(pMem->GetData(), pMem->GetSize());
    Disassembler::decode_buf(disasmBuf, m_disasm, pMem->GetAddress(), 10);
    m_pc = m_pTargetModel->GetPC();

    emit beginResetModel();
    emit endResetModel();
    //emit modelReset(QPrivateSignal());
}

