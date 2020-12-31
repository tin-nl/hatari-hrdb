#include "breakpointswidget.h"

#include <iostream>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QStringListModel>
#include <QFontDatabase>
#include <QCompleter>

#include "dispatcher.h"
#include "targetmodel.h"
#include "stringparsers.h"
#include "symboltablemodel.h"

BreakpointsTableModel::BreakpointsTableModel(QObject *parent, TargetModel *pTargetModel, Dispatcher* pDispatcher) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    connect(m_pTargetModel, &TargetModel::breakpointsChangedSignal, this, &BreakpointsTableModel::breakpointsChangedSlot);
}

int BreakpointsTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_pTargetModel->GetBreakpoints().m_breakpoints.size();
}

int BreakpointsTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return kColCount;
}

QVariant BreakpointsTableModel::data(const QModelIndex &index, int role) const
{
    uint32_t row = index.row();
    const Breakpoints& bps = m_pTargetModel->GetBreakpoints();
    if (role == Qt::DisplayRole)
    {
        if (index.column() == kColId)
        {
            return QString::number(row + 1);
        }
        else if (index.column() == kColExpression)
        {
            if (row >= bps.m_breakpoints.size())
                return QVariant();
            return QString(bps.m_breakpoints[row].m_expression.c_str());
        }
    }
    return QVariant(); // invalid item
}

QVariant BreakpointsTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Orientation::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            switch (section)
            {
            case kColId:         return QString(tr("ID"));
            case kColExpression: return QString(tr("Expression"));
            }
        }
        if (role == Qt::TextAlignmentRole)
        {
            return Qt::AlignLeft;
        }
    }
    return QVariant();
}

void BreakpointsTableModel::breakpointsChangedSlot()
{
    // TODO: I still don't understand how to update it here
    //const Breakpoints& bps = m_pTargetModel->GetBreakpoints();
    //emit dataChanged(this->createIndex(0, 0), this->createIndex(bps.m_breakpoints.size() - 1, 1));
    emit beginResetModel();
    emit endResetModel();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BreakpointsTableView::BreakpointsTableView(QWidget* parent, BreakpointsTableModel* pModel, TargetModel* pTargetModel) :
    QTableView(parent),
    m_pTableModel(pModel),
    //m_rightClickMenu(this),
    m_rightClickRow(-1)
{
    // Actions for right-click menu
    //m_pRunUntilAction = new QAction(tr("Run to here"), this);
    //connect(m_pRunUntilAction, &QAction::triggered, this, &BreakpointsTableView::runToCursorRightClick);
    //m_pBreakpointAction = new QAction(tr("Toggle Breakpoint"), this);
    //m_rightClickMenu.addAction(m_pRunUntilAction);
    //m_rightClickMenu.addAction(m_pBreakpointAction);
    //new QShortcut(QKeySequence(tr("F3", "Run to cursor")),        this, SLOT(runToCursor()));
    //new QShortcut(QKeySequence(tr("F9", "Toggle breakpoint")),    this, SLOT(toggleBreakpoint()));

    //connect(m_pBreakpointAction, &QAction::triggered,                  this, &BreakpointsTableView::toggleBreakpointRightClick);
    //connect(pTargetModel,        &TargetModel::startStopChangedSignal, this, &BreakpointsTableView::RecalcRowCount);

    // This table gets the focus from the parent docking widget
    setFocus();
}

/*
void BreakpointsTableView::contextMenuEvent(QContextMenuEvent *event)
{
    QModelIndex index = this->indexAt(event->pos());
    if (!index.isValid())
        return;

    m_rightClickRow = index.row();
    m_rightClickMenu.exec(event->globalPos());

}

void BreakpointsTableView::runToCursorRightClick()
{
    m_pTableModel->RunToRow(m_rightClickRow);
    m_rightClickRow = -1;
}

void BreakpointsTableView::toggleBreakpointRightClick()
{
    m_pTableModel->ToggleBreakpoint(m_rightClickRow);
    m_rightClickRow = -1;
}

void BreakpointsTableView::runToCursor()
{
    // How do we get the selected row
    QModelIndex i = this->currentIndex();
    m_pTableModel->RunToRow(i.row());
}

void BreakpointsTableView::toggleBreakpoint()
{
    // How do we get the selected row
    QModelIndex i = this->currentIndex();
    m_pTableModel->ToggleBreakpoint(i.row());
}
*/
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BreakpointsWidget::BreakpointsWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDockWidget(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    this->setWindowTitle("Breakpoints");

    // Make the data first
    pModel = new BreakpointsTableModel(this, pTargetModel, pDispatcher);

    m_pTableView = new BreakpointsTableView(this, pModel, m_pTargetModel);
    m_pTableView->setModel(pModel);

    m_pSymbolTableModel = new SymbolTableModel(this, m_pTargetModel->GetSymbolTable());
    QCompleter* pCompl = new QCompleter(m_pSymbolTableModel, this);
    pCompl->setCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_pTableView->setFont(monoFont);
    QFontMetrics fm(monoFont);

    // Down the side
    m_pTableView->verticalHeader()->hide();
    m_pTableView->verticalHeader()->setDefaultSectionSize(fm.height());

    // Layouts
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    QHBoxLayout* pTopLayout = new QHBoxLayout;
    auto pMainRegion = new QWidget(this);   // whole panel
    auto pTopRegion = new QWidget(this);      // top buttons/edits

    pMainLayout->addWidget(pTopRegion);
    pMainLayout->addWidget(m_pTableView);

    pTopRegion->setLayout(pTopLayout);
    pMainRegion->setLayout(pMainLayout);
    setWidget(pMainRegion);
}

