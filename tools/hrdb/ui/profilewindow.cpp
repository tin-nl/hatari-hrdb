#include "profilewindow.h"

#include <iostream>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QSettings>
#include <QDebug>
#include <QPushButton>

#include "../transport/dispatcher.h"
#include "../models/targetmodel.h"
#include "../models/session.h"
#include "../models/profiledata.h"
#include "quicklayout.h"

//-----------------------------------------------------------------------------
ProfileTableModel::ProfileTableModel(QObject *parent, TargetModel *pTargetModel, Dispatcher* pDispatcher) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    connect(m_pTargetModel, &TargetModel::profileChangedSignal,     this, &ProfileTableModel::profileChangedSlot);
    connect(m_pTargetModel, &TargetModel::symbolTableChangedSignal, this, &ProfileTableModel::symbolChangedSlot);
}

//-----------------------------------------------------------------------------
int ProfileTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return entries.size();
}

//-----------------------------------------------------------------------------
int ProfileTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return kColCount;
}

//-----------------------------------------------------------------------------
QVariant ProfileTableModel::data(const QModelIndex &index, int role) const
{
    int32_t row = index.row();
    const Entry& ent = entries[row];

    if (role == Qt::DisplayRole)
    {
        if (index.column() == kColAddress)
            return ent.address;
        else if (index.column() == kColInstructionCount)
            return ent.instructionCount;
        else if (index.column() == kColCycles)
            return QVariant(static_cast<qlonglong>(ent.cycleCount));
    }
    if (role == Qt::TextAlignmentRole)
    {
        if (index.column() == kColAddress)
            return Qt::AlignLeft;
        return Qt::AlignRight;

    }
    return QVariant(); // invalid item
}

//-----------------------------------------------------------------------------
QVariant ProfileTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Orientation::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            switch (section)
            {
            case kColAddress:           return QString(tr("Name"));
            case kColInstructionCount:  return QString(tr("Instructions"));
            case kColCycles:            return QString(tr("Cycles"));
            }
        }
        if (role == Qt::TextAlignmentRole)
        {
            if (section == kColAddress)
                return Qt::AlignLeft;
            return Qt::AlignRight;
        }
    }
    return QVariant();
}

//-----------------------------------------------------------------------------
void ProfileTableModel::profileChangedSlot()
{
    updateRows();
}

//-----------------------------------------------------------------------------
void ProfileTableModel::symbolChangedSlot()
{
    updateRows();
}

//-----------------------------------------------------------------------------
void ProfileTableModel::updateRows()
{
    map.clear();
    QString text;
    const ProfileData& data = m_pTargetModel->GetRawProfileData();
    const SymbolTable& symbols = m_pTargetModel->GetSymbolTable();
    Symbol result;

    for (const ProfileData::Pair ent : data.m_entries)
    {
        if (symbols.FindLowerOrEqual(ent.first, result))
        {
            QMap<uint32_t, Entry>::iterator it = map.find(result.address);

            if (it == map.end())
            {
                Entry entry;
                entry.address = QString::fromStdString(result.name);
                entry.instructionCount = ent.second.count;
                entry.cycleCount = ent.second.cycles;
                map.insert(result.address, entry);
            }
            else {
                it->instructionCount += ent.second.count;
                it->cycleCount += ent.second.cycles;
            }
        }
    }

    emit beginResetModel();
    entries.clear();
    for (auto it : map)
    {
        entries.push_back(it);
    }
    emit endResetModel();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
ProfileTableView::ProfileTableView(QWidget* parent, ProfileTableModel* pModel) :
    QTableView(parent),
    m_pTableModel(pModel),
    m_rightClickRow(-1)
{
    // This table gets the focus from the parent docking widget
    setFocus();
}

//-----------------------------------------------------------------------------
ProfileTableView::~ProfileTableView()
{

}

//-----------------------------------------------------------------------------
ProfileWindow::ProfileWindow(QWidget *parent, Session* pSession) :
    QDockWidget(parent),
    m_pSession(pSession),
    m_pTargetModel(pSession->m_pTargetModel),
    m_pDispatcher(pSession->m_pDispatcher)
{
    this->setWindowTitle("Profile");
    setObjectName("Profile");

    // Layouts
    QVBoxLayout* pMainLayout = new QVBoxLayout;
    QHBoxLayout* pTopLayout = new QHBoxLayout;
    auto pMainRegion = new QWidget(this);   // whole panel
    auto pTopRegion = new QWidget(this);      // top buttons/edits

    m_pStartStopButton = new QPushButton("Start", this);
    m_pResetButton = new QPushButton("Reset", this);
    m_pTableModel = new ProfileTableModel(this, m_pTargetModel, m_pDispatcher);
    m_pTableView = new ProfileTableView(this, m_pTableModel);
    m_pTableView->setModel(m_pTableModel);

    pTopLayout->addWidget(m_pStartStopButton);
    pTopLayout->addWidget(m_pResetButton);
    pMainLayout->addWidget(pTopRegion);
    pMainLayout->addWidget(m_pTableView);

    SetMargins(pTopLayout);
    SetMargins(pMainLayout);

    pTopRegion->setLayout(pTopLayout);
    pMainRegion->setLayout(pMainLayout);
    setWidget(pMainRegion);

    loadSettings();

    connect(m_pTargetModel,     &TargetModel::connectChangedSignal,     this, &ProfileWindow::connectChangedSlot);
    connect(m_pTargetModel,     &TargetModel::startStopChangedSignal,   this, &ProfileWindow::startStopChangedSlot);
    connect(m_pTargetModel,     &TargetModel::profileChangedSignal,     this, &ProfileWindow::profileChangedSlot);
    connect(m_pSession,         &Session::settingsChanged,              this, &ProfileWindow::settingsChangedSlot);

    connect(m_pStartStopButton, &QAbstractButton::clicked,              this, &ProfileWindow::startStopClicked);
    connect(m_pResetButton,     &QAbstractButton::clicked,              this, &ProfileWindow::resetClicked);

    // Refresh enable state
    connectChangedSlot();

    // Refresh font
    settingsChangedSlot();
}

ProfileWindow::~ProfileWindow()
{
}

void ProfileWindow::keyFocus()
{
    activateWindow();
}

void ProfileWindow::loadSettings()
{
    QSettings settings;
    settings.beginGroup("Profile");

    restoreGeometry(settings.value("geometry").toByteArray());
    settings.endGroup();
}

void ProfileWindow::saveSettings()
{
    QSettings settings;
    settings.beginGroup("Profile");

    settings.setValue("geometry", saveGeometry());
    settings.endGroup();
}

void ProfileWindow::connectChangedSlot()
{
    bool enable = m_pTargetModel->IsConnected() && !m_pTargetModel->IsRunning();
    m_pStartStopButton->setEnabled(enable);
    m_pResetButton->setEnabled(enable);
}

void ProfileWindow::startStopChangedSlot()
{
    bool enable = m_pTargetModel->IsConnected() && !m_pTargetModel->IsRunning();
    m_pStartStopButton->setEnabled(enable);
    m_pResetButton->setEnabled(enable);
}

void ProfileWindow::profileChangedSlot()
{
    if (m_pTargetModel->IsProfileEnabled())
    {
        m_pStartStopButton->setText("Stop");
    }
    else {
        m_pStartStopButton->setText("Start");
    }
}

void ProfileWindow::settingsChangedSlot()
{
    m_pTableView->setFont(m_pSession->GetSettings().m_font);
    QFontMetrics fm(m_pSession->GetSettings().m_font);
    // Down the side
    m_pTableView->verticalHeader()->hide();
    m_pTableView->verticalHeader()->setDefaultSectionSize(fm.height());
}

void ProfileWindow::startStopClicked()
{
    if (!m_pTargetModel->IsConnected())
        return;

    if (m_pTargetModel->IsProfileEnabled())
        m_pDispatcher->SendCommandPacket("profile 0");
    else
        m_pDispatcher->SendCommandPacket("profile 1");
}

void ProfileWindow::resetClicked()
{
    m_pTargetModel->ProfileReset();
}
