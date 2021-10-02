#include "hardwarewindow.h"

#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QSettings>
#include <QTreeView>
#include <QDebug>

#include "../transport/dispatcher.h"
#include "../models/targetmodel.h"
#include "../models/session.h"
#include "../hardware/hardware_st.h"
#include "../hardware/regs_st.h"
#include "quicklayout.h"

#define stringify(x) #x

#define EXTRACT_FIELD_ENUM(mem, reg, field)                            \
    if (mem.HasAddress(Regs::reg))                                     \
    {                                                                  \
        uint8_t byte = mem.ReadAddressByte(Regs::reg);                 \
        return Regs::GetString(Regs::GetField_##reg##_##field(byte));  \
    }                                                                  \
    return QString("no mem?");

#define EXTRACT_FIELD_DEC(mem, reg, field)                             \
    if (mem.HasAddress(Regs::reg))                                     \
    {                                                                  \
        uint8_t byte = mem.ReadAddressByte(Regs::reg);                 \
        return QString::asprintf("%u", Regs::GetField_##reg##_##field(byte));    \
    }                                                                  \
    return QString("no mem?");

#define ADD_MFP_FIELD_NAME(regVal, reg, field)     \
    if (Regs::GetField_##reg##_##field(regVal))    \
    { if(str.size()) str+= "|"; str += stringify(field); }

//-----------------------------------------------------------------------------
QString GetMfpBlockA(const Memory& mem, uint32_t regAddr)
{
    QString str;
    if (!mem.HasAddress(regAddr))
        return "";

    uint8_t regVal = mem.ReadAddressByte(regAddr);
    ADD_MFP_FIELD_NAME(regVal, MFP_IERA, MONO_DETECT)
    ADD_MFP_FIELD_NAME(regVal, MFP_IERA, RS232_RING )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERA, TIMER_A	)
    ADD_MFP_FIELD_NAME(regVal, MFP_IERA, REC_FULL   )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERA, REC_ERR	)
    ADD_MFP_FIELD_NAME(regVal, MFP_IERA, SEND_EMPTY )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERA, SEND_ERR   )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERA, TIMER_B	)
    return str;
}

//-----------------------------------------------------------------------------
QString GetMfpBlockB(const Memory& mem, uint32_t regAddr)
{
    QString str;
    if (!mem.HasAddress(regAddr))
        return "";

    uint8_t regVal = mem.ReadAddressByte(regAddr);
    ADD_MFP_FIELD_NAME(regVal, MFP_IERB, FDC_HDC	 )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERB, IKBD_MIDI   )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERB, TIMER_C	 )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERB, TIMER_D	 )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERB, BLITTER	 )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERB, RS232_CTS   )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERB, RS232_DTD   )
    ADD_MFP_FIELD_NAME(regVal, MFP_IERB, CENT_BUSY   )
    return str;
}

//-----------------------------------------------------------------------------
HardwareTreeItem::HardwareTreeItem(const char* headerName, uint32_t memTypes, Type type)
    : m_memTypes(memTypes),
      m_type(type),
      m_title(headerName)
{}

//-----------------------------------------------------------------------------
HardwareTreeItem::~HardwareTreeItem()
{
    qDeleteAll(m_childItems);
}

//-----------------------------------------------------------------------------
void HardwareTreeItem::appendChild(HardwareTreeItem *item)
{
    m_childItems.append(item);
    item->m_parentItem = this;
}

//-----------------------------------------------------------------------------
HardwareTreeItem *HardwareTreeItem::child(int row)
{
    if (row < 0 || row >= m_childItems.size())
        return nullptr;
    return m_childItems.at(row);
}

//-----------------------------------------------------------------------------
int HardwareTreeItem::childCount() const
{
    return m_childItems.count();
}

//-----------------------------------------------------------------------------
HardwareTreeItem *HardwareTreeItem::parentItem()
{
    return m_parentItem;
}

//-----------------------------------------------------------------------------
int HardwareTreeItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<HardwareTreeItem*>(this));

    return 0;
}

//-----------------------------------------------------------------------------
HardwareTableModel::HardwareTableModel(QObject *parent, TargetModel *pTargetModel, Dispatcher* pDispatcher) :
    QAbstractItemModel(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher),
    m_videoMem(0,0),
    m_mfpMem(0,0)
{
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &HardwareTableModel::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal,    this, &HardwareTableModel::memoryChangedSlot);

    m_pRootItem = new HardwareTreeItem("", 0, HardwareTreeItem::kHeader);

    HardwareTreeItem* m_pVideo = new HardwareTreeItem("Video", 0, HardwareTreeItem::kHeader);
    m_pVideo->appendChild(new HardwareTreeItem("Resolution", HardwareTreeItem::kMemTypeVideo, HardwareTreeItem::kVideoRes));
    m_pVideo->appendChild(new HardwareTreeItem("Sync Rate", HardwareTreeItem::kMemTypeVideo, HardwareTreeItem::kVideoHz));
    m_pVideo->appendChild(new HardwareTreeItem("Screen Base", HardwareTreeItem::kMemTypeVideo, HardwareTreeItem::kVideoBase));

    HardwareTreeItem* m_pMfp = new HardwareTreeItem("MFP", 0, HardwareTreeItem::kHeader);

    m_pMfp->appendChild(new HardwareTreeItem("IERA Interrupt Enable A",  HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpEnabledA  ));
    m_pMfp->appendChild(new HardwareTreeItem("IMRA Interrupt Mask A",    HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpMaskA     ));
    m_pMfp->appendChild(new HardwareTreeItem("IPRA Interrupt Pending A", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpPendingA  ));
    m_pMfp->appendChild(new HardwareTreeItem("ISRA Interrupt Service A", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpInServiceA));
    m_pMfp->appendChild(new HardwareTreeItem("IERB Interrupt Enable B",  HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpEnabledB  ));
    m_pMfp->appendChild(new HardwareTreeItem("IMRB Interrupt Mask B",    HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpMaskB     ));
    m_pMfp->appendChild(new HardwareTreeItem("IPRB Interrupt Pending B", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpPendingB  ));
    m_pMfp->appendChild(new HardwareTreeItem("ISRB Interrupt Service B", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpInServiceB));

    m_pMfp->appendChild(new HardwareTreeItem("TACR Timer A Mode", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpTimerAMode));
    m_pMfp->appendChild(new HardwareTreeItem("TADR Timer A Data", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpTimerAData));
    m_pMfp->appendChild(new HardwareTreeItem("TBCR Timer B Mode", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpTimerBMode));
    m_pMfp->appendChild(new HardwareTreeItem("TBDR Timer B Data", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpTimerBData));

    m_pMfp->appendChild(new HardwareTreeItem("TBCR Timer C Mode", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpTimerCMode));
    m_pMfp->appendChild(new HardwareTreeItem("TBDR Timer C Data", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpTimerCData));
    m_pMfp->appendChild(new HardwareTreeItem("TBCR Timer D Mode", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpTimerDMode));
    m_pMfp->appendChild(new HardwareTreeItem("TBDR Timer D Data", HardwareTreeItem::kMemTypeMfp, HardwareTreeItem::kMfpTimerDData));

    m_pRootItem->appendChild(m_pVideo);
    m_pRootItem->appendChild(m_pMfp);
}

//-----------------------------------------------------------------------------
HardwareTableModel::~HardwareTableModel()
{
    delete m_pRootItem;
}

//-----------------------------------------------------------------------------
int HardwareTableModel::columnCount(const QModelIndex & /*parent*/) const
{
    return 2;
}

//-----------------------------------------------------------------------------
QVariant HardwareTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    HardwareTreeItem *item = static_cast<HardwareTreeItem*>(index.internalPointer());
    if (index.column() == 0)
        return item->m_title;
    if (index.column() == 1)
    {
        return getData(item->m_type);
    }

    return QVariant();
}

//-----------------------------------------------------------------------------
Qt::ItemFlags HardwareTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return QAbstractItemModel::flags(index);
}

//-----------------------------------------------------------------------------
QVariant HardwareTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    (void)section;
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        return QVariant();
    }
    return QVariant();
}

//-----------------------------------------------------------------------------
QModelIndex HardwareTableModel::index(int row, int column, const QModelIndex & parent) const
{
    if (!hasIndex(row, column, parent))
         return QModelIndex();

     HardwareTreeItem *parentItem;

     if (!parent.isValid())
         parentItem = m_pRootItem;
     else
         parentItem = static_cast<HardwareTreeItem*>(parent.internalPointer());

     HardwareTreeItem *childItem = parentItem->child(row);
     if (childItem)
         return createIndex(row, column, childItem);
     return QModelIndex();
}

//-----------------------------------------------------------------------------
QModelIndex HardwareTableModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return QModelIndex();

    HardwareTreeItem *childItem = static_cast<HardwareTreeItem*>(index.internalPointer());
    HardwareTreeItem *parentItem = childItem->parentItem();

    if (parentItem == m_pRootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

//-----------------------------------------------------------------------------
int HardwareTableModel::rowCount(const QModelIndex &parent) const
{
    HardwareTreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_pRootItem;
    else
        parentItem = static_cast<HardwareTreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

//-----------------------------------------------------------------------------
void HardwareTableModel::startStopChangedSlot()
{
    if (m_pTargetModel->IsRunning())
    {

    }
    else {
        m_videoRequest = m_pDispatcher->RequestMemory(MemorySlot::kHardwareWindow, Regs::VID_REG_BASE,  0x70);
        m_mfpRequest   = m_pDispatcher->RequestMemory(MemorySlot::kHardwareWindow, Regs::MFP_GPIP,      0x30);
    }
}

//-----------------------------------------------------------------------------
void HardwareTableModel::memoryChangedSlot(int memorySlot, uint64_t commandId)
{
    (void) memorySlot;
    if (commandId == m_videoRequest)
    {
        m_videoMem = *m_pTargetModel->GetMemory(MemorySlot::kHardwareWindow);
        emitDataChange(m_pRootItem, HardwareTreeItem::kMemTypeVideo);
    }
    else if (commandId == m_mfpRequest)
    {
        m_mfpMem = *m_pTargetModel->GetMemory(MemorySlot::kHardwareWindow);
        emitDataChange(m_pRootItem, HardwareTreeItem::kMemTypeMfp);
    }
}

//-----------------------------------------------------------------------------
void HardwareTableModel::emitDataChange(HardwareTreeItem* root, uint32_t memTypes)
{
    if (root->m_memTypes & memTypes)
        emit dataChanged(createIndex(0, 0, root), createIndex(0, 1, root));

    for (int i = 0; i < root->childCount(); ++i)
        emitDataChange(root->child(i), memTypes);
}

//-----------------------------------------------------------------------------
QString HardwareTableModel::getData(HardwareTreeItem::Type type) const
{
    switch (type)
    {
    case HardwareTreeItem::Type::kHeader:
        return QString("");
    case HardwareTreeItem::Type::kVideoRes:
        EXTRACT_FIELD_ENUM(m_videoMem, VID_SHIFTER_RES, RES);
    case HardwareTreeItem::Type::kVideoHz:
        EXTRACT_FIELD_ENUM(m_videoMem, VID_SYNC_MODE, RATE);
    case HardwareTreeItem::Type::kVideoBase:
        {
            uint32_t address = 0;
            HardwareST::GetVideoBase(m_videoMem, m_pTargetModel->GetMachineType(), address);
            return QString::asprintf("$%x", address);
        }
    case HardwareTreeItem::Type::kMfpEnabledA:
        return GetMfpBlockA(m_mfpMem, Regs::MFP_IERA);
    case HardwareTreeItem::Type::kMfpMaskA:
        return GetMfpBlockA(m_mfpMem, Regs::MFP_IMRA);
    case HardwareTreeItem::Type::kMfpPendingA:
        return GetMfpBlockA(m_mfpMem, Regs::MFP_IPRA);
    case HardwareTreeItem::Type::kMfpInServiceA:
        return GetMfpBlockA(m_mfpMem, Regs::MFP_ISRA);
    case HardwareTreeItem::Type::kMfpEnabledB:
        return GetMfpBlockB(m_mfpMem, Regs::MFP_IERB);
    case HardwareTreeItem::Type::kMfpMaskB:
        return GetMfpBlockB(m_mfpMem, Regs::MFP_IMRB);
    case HardwareTreeItem::Type::kMfpPendingB:
        return GetMfpBlockB(m_mfpMem, Regs::MFP_IPRB);
    case HardwareTreeItem::Type::kMfpInServiceB:
        return GetMfpBlockB(m_mfpMem, Regs::MFP_ISRB);

    // Timer A/B/C/D
    case HardwareTreeItem::Type::kMfpTimerAMode:
        EXTRACT_FIELD_ENUM(m_mfpMem, MFP_TACR, MODE_TIMER_A);
    case HardwareTreeItem::Type::kMfpTimerAData:
        EXTRACT_FIELD_DEC(m_mfpMem, MFP_TADR, ALL);
    case HardwareTreeItem::Type::kMfpTimerBMode:
        EXTRACT_FIELD_ENUM(m_mfpMem, MFP_TBCR, MODE_TIMER_B);
    case HardwareTreeItem::Type::kMfpTimerBData:
        EXTRACT_FIELD_DEC(m_mfpMem, MFP_TBDR, ALL);
    case HardwareTreeItem::Type::kMfpTimerCMode:
        EXTRACT_FIELD_ENUM(m_mfpMem, MFP_TCDCR, MODE_TIMER_C);
    case HardwareTreeItem::Type::kMfpTimerCData:
        EXTRACT_FIELD_DEC(m_mfpMem, MFP_TCDR, ALL);
    case HardwareTreeItem::Type::kMfpTimerDMode:
        EXTRACT_FIELD_ENUM(m_mfpMem, MFP_TCDCR, MODE_TIMER_D);
    case HardwareTreeItem::Type::kMfpTimerDData:
        EXTRACT_FIELD_DEC(m_mfpMem, MFP_TDDR, ALL);
    }
    return QString();
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

    m_pTableView = new QTreeView(this);
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

    m_pTableView->expandAll();
    m_pTableView->resizeColumnToContents(0);
    m_pTableView->resizeColumnToContents(1);
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

