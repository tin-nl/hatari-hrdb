#include "hardwarewindow.h"

#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QSettings>
#include <QDebug>

#include "../transport/dispatcher.h"
#include "../models/targetmodel.h"
#include "../models/session.h"
#include "quicklayout.h"

//-----------------------------------------------------------------------------
HardwareTableModel::HardwareTableModel(QObject *parent, TargetModel *pTargetModel, Dispatcher* pDispatcher) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &HardwareTableModel::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal,    this, &HardwareTableModel::memoryChangedSlot);
}

int HardwareTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return 10;
}

int HardwareTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return kColCount;
}

QVariant HardwareTableModel::data(const QModelIndex &index, int role) const
{
    uint32_t row = index.row();
    const Breakpoints& bps = m_pTargetModel->GetBreakpoints();
    if (row >= bps.m_breakpoints.size())
        return QVariant();
    const Breakpoint& bp = bps.m_breakpoints[row];

    if (role == Qt::DisplayRole)
    {
        if (index.column() == kColName)
            return QString("name");
        else if (index.column() == kColData)
            return QString("data");
    }
    if (role == Qt::TextAlignmentRole)
    {
        if (index.column() == kColName)
            return Qt::AlignLeft;
        return Qt::AlignRight;
    }
    return QVariant(); // invalid item
}

QVariant HardwareTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Orientation::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            switch (section)
            {
            }
        }
        if (role == Qt::TextAlignmentRole)
        {
            if (section == kColName)
                return Qt::AlignLeft;
            return Qt::AlignRight;
        }
    }
    return QVariant();
}

//-----------------------------------------------------------------------------
void HardwareTableModel::startStopChangedSlot()
{

}


//-----------------------------------------------------------------------------
void HardwareTableModel::memoryChangedSlot(int memorySlot, uint64_t commandId)
{

}

//-----------------------------------------------------------------------------
HardwareWindow::HardwareWindow(QWidget *parent, Session* pSession) :
    QDockWidget(parent),
    m_pSession(pSession),
    m_pTargetModel(pSession->m_pTargetModel),
    m_pDispatcher(pSession->m_pDispatcher)
{
    this->setWindowTitle("Hardware");
    setObjectName("Hardware");

    m_pTableView = new QTableView(this);
    m_pTableView->setModel(new HardwareTableModel(this, m_pTargetModel, m_pDispatcher));

    // Layouts
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    QHBoxLayout* pTopLayout = new QHBoxLayout;
    auto pMainRegion = new QWidget(this);   // whole panel
    auto pTopRegion = new QWidget(this);      // top buttons/edits

    SetMargins(pTopLayout);
    pTopLayout->addWidget(m_pTableView);
    SetMargins(pMainLayout);
    pMainLayout->addWidget(pTopRegion);

    pTopRegion->setLayout(pTopLayout);
    pMainRegion->setLayout(pMainLayout);
    setWidget(pMainRegion);

    loadSettings();

    connect(m_pTargetModel,  &TargetModel::connectChangedSignal, this, &HardwareWindow::connectChangedSlot);
    connect(m_pSession,      &Session::settingsChanged,          this, &HardwareWindow::settingsChangedSlot);

    // Refresh enable state
    connectChangedSlot();

    // Refresh font
    settingsChangedSlot();
}

HardwareWindow::~HardwareWindow()
{
}

void HardwareWindow::keyFocus()
{
    activateWindow();
    m_pTableView->setFocus();
}

void HardwareWindow::loadSettings()
{
    QSettings settings;
    settings.beginGroup("Console");

    restoreGeometry(settings.value("geometry").toByteArray());
    settings.endGroup();
}

void HardwareWindow::saveSettings()
{
    QSettings settings;
    settings.beginGroup("Hardware");

    settings.setValue("geometry", saveGeometry());
    settings.endGroup();
}

void HardwareWindow::connectChangedSlot()
{
    bool enable = m_pTargetModel->IsConnected();
    m_pTableView->setEnabled(enable);

    if (m_pTargetModel->IsConnected())
    {
        if (m_pTargetModel->IsConnected())
        {
        }
    }
    else
    {
    }
}

void HardwareWindow::settingsChangedSlot()
{
}

