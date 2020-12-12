#include "memoryviewwidget.h"

#include <iostream>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QStringListModel>
#include <QFontDatabase>

#include "dispatcher.h"
#include "targetmodel.h"

MemoryViewWidget::MemoryViewWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDockWidget(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    this->setWindowTitle("Memory");

    QVBoxLayout *layout = new QVBoxLayout;
    auto pGroupBox = new QGroupBox(this);

    m_pLineEdit = new QLineEdit(this);
    m_pTableView = new QTableView(this);

    MemoryViewTableModel* pModel = new MemoryViewTableModel(this, pTargetModel);
    m_pTableView->setModel(pModel);

    m_pTableView->horizontalHeader()->setMinimumSectionSize(0);
    m_pTableView->horizontalHeader()->hide();
    m_pTableView->setColumnWidth(0, 9*8);
    m_pTableView->setColumnWidth(1, 500);

    m_pTableView->verticalHeader()->hide();
    //m_pTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_pTableView->verticalHeader()->setDefaultSectionSize(16);

    layout->addWidget(m_pLineEdit);
    layout->addWidget(m_pTableView);
    pGroupBox->setLayout(layout);
    setWidget(pGroupBox);

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_pTableView->setFont(monoFont);
    m_pTableView->resizeRowsToContents();

    // Listen for start/stop, so we can update our memory request
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal,   this, &MemoryViewWidget::startStopChangedSlot);
    connect(m_pLineEdit, &QLineEdit::textChanged,                   this, &MemoryViewWidget::textEditChangedSlot);
}

void MemoryViewWidget::startStopChangedSlot()
{
    // Request new memory for the view
    if (!m_pTargetModel->IsRunning())
    {
        m_pDispatcher->RequestMemory(MemorySlot::kMemoryView, m_pLineEdit->text().toStdString(), "100");
    }
}

void MemoryViewWidget::textEditChangedSlot()
{
    m_pDispatcher->RequestMemory(MemorySlot::kMemoryView, m_pLineEdit->text().toStdString(), "100");
}

MemoryViewTableModel::MemoryViewTableModel(QObject *parent, TargetModel *pTargetModel) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_address(0)
{
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal, this, &MemoryViewTableModel::memoryChangedSlot);
}

int MemoryViewTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

int MemoryViewTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return 3;
}

QVariant MemoryViewTableModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (index.column() == 0)
        {
            QString addr = QString::asprintf("%08x", m_address + 16 * index.row());
            return addr;
        }
        else if (index.column() == 1)
        {
            return m_rows[index.row()];
        }
    }
    return QVariant(); // invalid item
}

void MemoryViewTableModel::memoryChangedSlot(int memorySlot, uint64_t commandId)
{
    if (memorySlot != MemorySlot::kMemoryView)
        return;

    m_rows.clear();
    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kMemoryView);
    if (!pMem)
        return;

    // Fetch underlying data
    buffer_reader MemoryViewBuf(pMem->GetData(), pMem->GetSize());

    // We should just save the memory block here and format on demand
    // Build up the text area
    uint32_t rowCount = (pMem->GetSize() + 15) / 16;
    uint32_t offset = 0;
    m_address = pMem->GetAddress();

    for (uint32_t r = 0; r < rowCount; ++r)
    {
        QString row_text;

        for (uint32_t c = 0; c < 16; ++c)
        {
            if (offset == pMem->GetSize())
                break;

            QString pc_text = QString::asprintf("%02x ", pMem->Get(offset));
            row_text += pc_text;
            ++offset;
        }
        m_rows.push_back(row_text);
    }

    emit beginResetModel();
    emit endResetModel();
}

