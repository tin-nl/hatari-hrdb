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
#include "stringparsers.h"


MemoryViewTableModel::MemoryViewTableModel(QObject *parent, TargetModel *pTargetModel, Dispatcher* pDispatcher) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher),
    m_address(0),
    m_rowCount(1),
    m_requestId(0)
{
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal,      this, &MemoryViewTableModel::memoryChangedSlot);
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal,   this, &MemoryViewTableModel::startStopChangedSlot);
}

void MemoryViewTableModel::SetAddress(uint32_t address)
{
    m_address = address;
    RequestMemory();
}

void MemoryViewTableModel::SetRowCount(uint32_t rowCount)
{
    if (rowCount != m_rowCount)
    {
        emit beginResetModel();
        m_rowCount = rowCount;
        RequestMemory();
        emit endResetModel();
    }
}

void MemoryViewTableModel::MoveUp()
{
    if (m_requestId != 0)
        return; // not up to date

    if (m_address > 16)
        SetAddress(m_address - 16);
    else
        SetAddress(0);
}

void MemoryViewTableModel::MoveDown()
{
    if (m_requestId != 0)
        return; // not up to date

    SetAddress(m_address + 16);
}

void MemoryViewTableModel::PageUp()
{
    if (m_requestId != 0)
        return; // not up to date

    if (m_address > 16 * m_rowCount)
        SetAddress(m_address - 16 * m_rowCount);
    else
        SetAddress(0);
}

void MemoryViewTableModel::PageDown()
{
    if (m_requestId != 0)
        return; // not up to date

    SetAddress(m_address + 16 * m_rowCount);
}

int MemoryViewTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rowCount;
}

int MemoryViewTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return 3;
}

QVariant MemoryViewTableModel::data(const QModelIndex &index, int role) const
{
    uint32_t row = index.row();
    if (role == Qt::DisplayRole)
    {
        if (row >= m_rows.size())
            return QVariant();

        if (index.column() == 0)
        {
            QString addr = QString::asprintf("%08x", m_address + 16 * row);
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

    // ignore out-of-date requests
    if (commandId != m_requestId)
        return;

    m_rows.clear();
    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kMemoryView);
    if (!pMem)
        return;

    if (pMem->GetAddress() != m_address)
        return;

    // Fetch underlying data
    buffer_reader MemoryViewBuf(pMem->GetData(), pMem->GetSize());

    // We should just save the memory block here and format on demand
    // Build up the text area
    uint32_t rowCount = (pMem->GetSize() + 15) / 16;
    uint32_t offset = 0;
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

    m_requestId = 0;    // flag request is complete
    emit dataChanged(this->createIndex(0, 0), this->createIndex(m_rowCount - 1, 1));
}

void MemoryViewTableModel::startStopChangedSlot()
{
    // Request new memory for the view
    if (!m_pTargetModel->IsRunning())
    {
        RequestMemory();
    }
}

void MemoryViewTableModel::RequestMemory()
{
    uint32_t size = ((m_rowCount * 16));
    m_requestId = m_pDispatcher->RequestMemory(MemorySlot::kMemoryView, m_address, size);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
MemoryTableView::MemoryTableView(QWidget* parent, MemoryViewTableModel* pModel, TargetModel* pTargetModel) :
    QTableView(parent),
    m_pTableModel(pModel),
    //m_rightClickMenu(this),
    m_rightClickRow(-1)
{
    // Actions for right-click menu
    //m_pRunUntilAction = new QAction(tr("Run to here"), this);
    //connect(m_pRunUntilAction, &QAction::triggered, this, &MemoryTableView::runToCursorRightClick);
    //m_pBreakpointAction = new QAction(tr("Toggle Breakpoint"), this);
    //m_rightClickMenu.addAction(m_pRunUntilAction);
    //m_rightClickMenu.addAction(m_pBreakpointAction);
    //new QShortcut(QKeySequence(tr("F3", "Run to cursor")),        this, SLOT(runToCursor()));
    //new QShortcut(QKeySequence(tr("F9", "Toggle breakpoint")),    this, SLOT(toggleBreakpoint()));

    //connect(m_pBreakpointAction, &QAction::triggered,                  this, &MemoryTableView::toggleBreakpointRightClick);
    connect(pTargetModel,        &TargetModel::startStopChangedSignal, this, &MemoryTableView::RecalcRowCount);

    // This table gets the focus from the parent docking widget
    setFocus();
}

/*
void MemoryTableView::contextMenuEvent(QContextMenuEvent *event)
{
    QModelIndex index = this->indexAt(event->pos());
    if (!index.isValid())
        return;

    m_rightClickRow = index.row();
    m_rightClickMenu.exec(event->globalPos());

}

void MemoryTableView::runToCursorRightClick()
{
    m_pTableModel->RunToRow(m_rightClickRow);
    m_rightClickRow = -1;
}

void MemoryTableView::toggleBreakpointRightClick()
{
    m_pTableModel->ToggleBreakpoint(m_rightClickRow);
    m_rightClickRow = -1;
}

void MemoryTableView::runToCursor()
{
    // How do we get the selected row
    QModelIndex i = this->currentIndex();
    m_pTableModel->RunToRow(i.row());
}

void MemoryTableView::toggleBreakpoint()
{
    // How do we get the selected row
    QModelIndex i = this->currentIndex();
    m_pTableModel->ToggleBreakpoint(i.row());
}
*/
QModelIndex MemoryTableView::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
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

void MemoryTableView::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);
    RecalcRowCount();
}

void MemoryTableView::RecalcRowCount()
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
MemoryViewWidget::MemoryViewWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDockWidget(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    this->setWindowTitle("Memory");

    // Make the data first
    pModel = new MemoryViewTableModel(this, pTargetModel, pDispatcher);

    QVBoxLayout *layout = new QVBoxLayout;
    auto pGroupBox = new QGroupBox(this);

    m_pLineEdit = new QLineEdit(this);
    m_pTableView = new MemoryTableView(this, pModel, m_pTargetModel);
    m_pTableView->setModel(pModel);

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_pTableView->setFont(monoFont);
    QFontMetrics fm(monoFont);

    m_pTableView->horizontalHeader()->hide();

    // Factor in max width and the edge margins
    int l;
    int r;
    int charWidth = fm.maxWidth();
    m_pTableView->getContentsMargins(&l, nullptr, &r, nullptr);
    m_pTableView->setColumnWidth(0, l + r + charWidth * 9);
    m_pTableView->setColumnWidth(1, l + r + charWidth * 16 * 3);

    // Down the side
    m_pTableView->verticalHeader()->hide();
    m_pTableView->verticalHeader()->setDefaultSectionSize(fm.height());

    layout->addWidget(m_pLineEdit);
    layout->addWidget(m_pTableView);
    pGroupBox->setLayout(layout);
    setWidget(pGroupBox);

    // Listen for start/stop, so we can update our memory request
    connect(m_pLineEdit, &QLineEdit::textChanged,                   this, &MemoryViewWidget::textEditChangedSlot);
}

void MemoryViewWidget::textEditChangedSlot()
{
    uint32_t addr;
    if (!StringParsers::ParseExpression(m_pLineEdit->text().toStdString().c_str(), addr,
                                        m_pTargetModel->GetSymbolTable(),
                                        m_pTargetModel->GetRegs()))
        return;
    pModel->SetAddress(addr);
}
