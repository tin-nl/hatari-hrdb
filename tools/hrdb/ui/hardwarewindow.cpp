#include "hardwarewindow.h"

#include <QVBoxLayout>
#include <QTreeView>
#include <QSettings>
#include <QDebug>

#include "../transport/dispatcher.h"
#include "../models/targetmodel.h"
#include "../models/session.h"
#include "../hardware/hardware_st.h"
#include "../hardware/regs_st.h"
#include "nonantialiasimage.h"
#include "quicklayout.h"
#include "showaddressactions.h"
#include "symboltext.h"

//-----------------------------------------------------------------------------
bool GetField(const Memory& mem, const Regs::FieldDef& def, QString& result)
{
    if (!mem.HasAddressMulti(def.regAddr, def.size))
        return false;

    uint32_t regVal = mem.ReadAddressMulti(def.regAddr, def.size);
    uint32_t extracted = (regVal >> def.shift) & def.mask;
    if (def.strings)
        result = QString::asprintf("%s ($%x)", GetString(def.strings, extracted), extracted);
    else
        result = QString::asprintf("%u ($%x)", extracted, extracted);
    return true;
}

//-----------------------------------------------------------------------------
bool GetFieldVal(const Memory& mem, const Regs::FieldDef& def, uint32_t& result)
{
    if (!mem.HasAddressMulti(def.regAddr, def.size))
        return false;

    uint32_t regVal = mem.ReadAddressMulti(def.regAddr, def.size);
    uint32_t extracted = (regVal >> def.shift) & def.mask;
    result = extracted;
    return true;
}

//-----------------------------------------------------------------------------
class HardwareBase
{
public:
    HardwareBase() :
        m_changed(false),
        m_pParent(nullptr),
        m_rowIndex(0)
    {}

    virtual ~HardwareBase();
    virtual bool isHeader() const { return false; }
    HardwareBase* AddChild(HardwareBase* pField)
    {
        pField->m_pParent = this;
        pField->m_rowIndex = this->m_children.size();

        m_children.push_back(pField);
        return pField;
    }

    QString                    m_title;
    QString                    m_text;
    bool                       m_changed;

    HardwareBase*              m_pParent;
    QVector<HardwareBase*>     m_children;
    int                        m_rowIndex;
};

//-----------------------------------------------------------------------------
class HardwareField : public HardwareBase
{
public:
    HardwareField()
    {
    }
    virtual ~HardwareField();
    virtual bool isHeader() const { return false; }
    virtual bool Update(const TargetModel* pTarget) = 0;
    virtual void UpdateSettings(const Session::Settings& settings);

protected:
    Session*    m_pSession;
};

//-----------------------------------------------------------------------------
class HardwareHeader : public HardwareBase
{
public:
    HardwareHeader(QString title, QString subTitle)
    {
        m_title = title;
        m_text = subTitle;
    }

    virtual ~HardwareHeader();
    virtual bool isHeader() const { return true; }
};

//-----------------------------------------------------------------------------
HardwareField::~HardwareField()
{
}

//-----------------------------------------------------------------------------
void HardwareField::UpdateSettings(const Session::Settings &settings)
{
//    m_pDefaultLabel->setFont(settings.m_font);
}

//-----------------------------------------------------------------------------
class HardwareFieldRegEnum : public HardwareField
{
public:
    HardwareFieldRegEnum(const Regs::FieldDef& def) :
        m_def(def)
    {
    }

    bool Update(const TargetModel* pTarget);

    const Regs::FieldDef&   m_def;
};

//-----------------------------------------------------------------------------
class HardwareFieldMultiField : public HardwareField
{
public:
    HardwareFieldMultiField(const Regs::FieldDef** defs) :
        m_pDefs(defs)
    {
    }

    bool Update(const TargetModel* pTarget);

    const Regs::FieldDef**  m_pDefs;
};

//-----------------------------------------------------------------------------
class HardwareFieldAddr : public HardwareField
{
public:
    enum Type
    {
        ScreenBase,
        ScreenCurr,
        BltSrc,
        BltDst,
        BasePage,
        Mfp
    };
    HardwareFieldAddr(Session* pSession, Type type, uint32_t address = 0);
    HardwareFieldAddr(Session* pSession, uint32_t address);
    ~HardwareFieldAddr();

    bool Update(const TargetModel* pTarget);
private:
    Type m_type;
    uint32_t m_address;
};

//-----------------------------------------------------------------------------
class HardwareFieldYm : public HardwareField
{
public:
    HardwareFieldYm(int index) :
        m_index(index)
    {
    }

    bool Update(const TargetModel* pTarget);
    int m_index;
};

//-----------------------------------------------------------------------------
class HardwareFieldYmPeriod : public HardwareField
{
public:
    HardwareFieldYmPeriod(int index) :
        m_index(index)
    {
    }

    bool Update(const TargetModel* pTarget);
    int m_index;
};

//-----------------------------------------------------------------------------
class HardwareFieldYmEnvShape : public HardwareField
{
public:
    HardwareFieldYmEnvShape() {}

    bool Update(const TargetModel* pTarget);
};

//-----------------------------------------------------------------------------
class HardwareFieldYmMixer : public HardwareField
{
public:
    HardwareFieldYmMixer() {}

    bool Update(const TargetModel* pTarget);
};

//-----------------------------------------------------------------------------
class HardwareFieldYmVolume : public HardwareField
{
public:
    HardwareFieldYmVolume(int index) :
        m_index(index)
    {
    }

    bool Update(const TargetModel* pTarget);
    int m_index;
};

//-----------------------------------------------------------------------------
class HardwareBitmap : public HardwareField
{
public:
    HardwareBitmap(Session* pSession);

    ~HardwareBitmap()
    {
        delete m_pImage;
    }

    NonAntiAliasImage*      m_pImage;
};

//-----------------------------------------------------------------------------
class HardwareBitmapBlitterHalftone : public HardwareBitmap
{
public:
    HardwareBitmapBlitterHalftone(Session* pSession) :
        HardwareBitmap(pSession)
    {
        m_pImage->setFixedSize(64, 64);
    }

    bool Update(const TargetModel* pTarget);
};

//-----------------------------------------------------------------------------
HardwareBase::~HardwareBase()
{
    for (int i = 0; i < m_children.size(); ++i)
        delete m_children[i];
}

HardwareHeader::~HardwareHeader()
{

}

//-----------------------------------------------------------------------------
bool HardwareFieldRegEnum::Update(const TargetModel* pTarget)
{
    const Memory& mem = *pTarget->GetMemory(MemorySlot::kHardwareWindow);
    QString str;
    if (!GetField(mem, m_def, str))
        return false;       // Wrong memory

    m_changed = m_text != str;
    m_text = str;
    return true;
}

//-----------------------------------------------------------------------------
bool HardwareFieldMultiField::Update(const TargetModel* pTarget)
{
    const Memory& mem = *pTarget->GetMemory(MemorySlot::kHardwareWindow);
    QString res;
    QTextStream ref(&res);
    const Regs::FieldDef** pDef = m_pDefs;
    for (; *pDef; ++pDef)
    {
        const Regs::FieldDef* pCurrDef = *pDef;
        if (!mem.HasAddress(pCurrDef->regAddr))
            return false;
        uint8_t regVal = mem.ReadAddressByte(pCurrDef->regAddr);
        uint16_t extracted = (regVal >> pCurrDef->shift) & pCurrDef->mask;

        if (pCurrDef->mask == 1 && !pCurrDef->strings)
        {
            // Single bit
            if (extracted)
            {
                ref << (*pDef)->name << " ";
            }
        }
        else {
            if (pCurrDef->strings)
            {
                const char* str = Regs::GetString(pCurrDef->strings, extracted);
                ref << pCurrDef->name << "=" << str << " ";
            }
            else
            {
                ref << pCurrDef->name << "=" << extracted << " ";
            }
        }
    }
    m_changed = m_text != res;
    m_text = res;
    return true;
}

//-----------------------------------------------------------------------------
HardwareFieldAddr::HardwareFieldAddr(Session* pSession, HardwareFieldAddr::Type type, uint32_t address) :
    m_type(type),
    m_address(address)
{
}

//-----------------------------------------------------------------------------
HardwareFieldAddr::HardwareFieldAddr(Session* pSession, uint32_t address) :
    m_type(BasePage),
    m_address(address)
{
}

//-----------------------------------------------------------------------------
HardwareFieldAddr::~HardwareFieldAddr()
{
}

//-----------------------------------------------------------------------------
bool HardwareFieldAddr::Update(const TargetModel* pTarget)
{
    const Memory& mem = *pTarget->GetMemory(MemorySlot::kHardwareWindow);
    const Memory* memB = pTarget->GetMemory(MemorySlot::kBasePage);

    uint32_t address = 0;
    bool valid = false;
    switch (m_type)
    {
    case Type::ScreenBase:
        valid = (HardwareST::GetVideoBase(mem, pTarget->GetMachineType(), address)); break;
    case Type::ScreenCurr:
        valid = (HardwareST::GetVideoCurrent(mem, address)); break;
    case Type::BltSrc:
        valid = (HardwareST::GetBlitterSrc(mem, pTarget->GetMachineType(), address)); break;
    case Type::BltDst:
        valid = (HardwareST::GetBlitterDst(mem, pTarget->GetMachineType(), address)); break;
    case Type::BasePage:
        if (memB)
        {
            valid = memB->HasAddressMulti(m_address, 4);
            if (valid)
                address = memB->ReadAddressMulti(m_address, 4);
        }
        break;
    case Type::Mfp:
        {
            const Memory* memMfp = pTarget->GetMemory(MemorySlot::kHardwareWindowMfpVecs);
            if (memMfp)
            {
                uint32_t base = memMfp->GetAddress();
                valid = memMfp->HasAddressMulti(base + m_address * 4, 4);
                if (valid)
                    address = memMfp->ReadAddressMulti(base + m_address * 4, 4);
            }
        }
        break;
    }
    if (valid)
    {
        address &= 0xffffff;
        QString str = QString::asprintf("$%08x", address);
        QString sym = DescribeSymbol(pTarget->GetSymbolTable(), address);
        if (!sym.isEmpty())
            str += " (" + sym + ")";
        m_changed = m_text != str;
        m_text = str;
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
bool HardwareFieldYm::Update(const TargetModel* pTarget)
{
    uint8_t val = pTarget->GetYm().m_regs[m_index];
    QString str = QString::asprintf("%u ($%x)", val, val);
    m_changed = m_text != str;
    m_text = str;
    return true;
}

//-----------------------------------------------------------------------------
bool HardwareFieldYmPeriod::Update(const TargetModel *pTarget)
{
    uint16_t valLo = (pTarget->GetYm().m_regs[m_index]);
    uint16_t valHi = (pTarget->GetYm().m_regs[m_index + 1]);
    uint16_t mask = 0xfff;
    double clock = 2000000.0;

    // Divisors are taken from the Datasheet.
    double divisor = clock / 16;

    if (m_index == Regs::YM_PERIOD_ENV_LO)
    {
        divisor = clock / 256;
        mask = 0xffff;
    }
    else if (m_index == Regs::YM_PERIOD_NOISE)
    {
        mask = 0x1f;
    }

    // Special cases
    uint32_t val = (valHi * 256U) | valLo;
    val &= mask;

    double hertz = divisor / (val ? val : 1);
    QString str = QString::asprintf("$%03x  Approx %.0fHz", val, hertz);
    m_changed = m_text != str;
    m_text = str;
    return true;
}

//-----------------------------------------------------------------------------
bool HardwareFieldYmEnvShape::Update(const TargetModel *pTarget)
{
    uint16_t val = (pTarget->GetYm().m_regs[Regs::YM_PERIOD_ENV_SHAPE]);
    const char* pString = Regs::GetString(static_cast<Regs::ENV_SHAPE>(val));
    QString str = pString;
    m_changed = m_text != str;
    m_text = str;
    return true;
}

//-----------------------------------------------------------------------------
bool HardwareFieldYmMixer::Update(const TargetModel *pTarget)
{
    uint32_t val = (pTarget->GetYm().m_regs[Regs::YM_MIXER]);

    QString str;
    if (!Regs::GetField_YM_MIXER_TONE_A_OFF(val))
        str += "SQUARE_A ";
    if (!Regs::GetField_YM_MIXER_TONE_B_OFF(val))
        str += "SQUARE_B ";
    if (!Regs::GetField_YM_MIXER_TONE_C_OFF(val))
        str += "SQUARE_C ";

    if (!Regs::GetField_YM_MIXER_NOISE_A_OFF(val))
        str += "NOISE_A ";
    if (!Regs::GetField_YM_MIXER_NOISE_B_OFF(val))
        str += "NOISE_B ";
    if (!Regs::GetField_YM_MIXER_NOISE_C_OFF(val))
        str += "NOISE_C ";

    m_changed = m_text != str;
    m_text = str;
    return true;
}

//-----------------------------------------------------------------------------
bool HardwareFieldYmVolume::Update(const TargetModel *pTarget)
{
    uint16_t val = pTarget->GetYm().m_regs[m_index];
    uint8_t squareVol = Regs::GetField_YM_VOLUME_A_VOL(val);
    bool useEnv = Regs::GetField_YM_VOLUME_A_ENVELOPE(val);

    QString str = QString::asprintf("Square Vol = %u%s",
                                    squareVol,
                                    useEnv ? " + ENVELOPE" : "");
    m_changed = m_text != str;
    m_text = str;
    return true;
}

HardwareBitmap::HardwareBitmap(Session *pSession)
{
    m_pImage = new NonAntiAliasImage(nullptr, pSession);
}

//-----------------------------------------------------------------------------
bool HardwareBitmapBlitterHalftone::Update(const TargetModel *pTarget)
{
    const Memory& mem = *pTarget->GetMemory(MemorySlot::kHardwareWindow);
    uint32_t address = Regs::BLT_HALFTONE_0;

    if (!mem.HasAddressMulti(address, 16 * 2U))
        return false;

    uint8_t* pData = m_pImage->AllocBitmap(16 * 16);
    for (uint y = 0; y < 16; ++y)
    {
        uint32_t data = mem.ReadAddressMulti(address + 2U * y, 2);
        for (int pix = 15; pix >= 0; --pix)
        {
            uint8_t val  = (data & 0x8000) ? 1 : 0;
            *pData++ = val;
            data <<= 1U;
        }
    }
    m_pImage->m_colours.resize(2U);
    m_pImage->m_colours[0] = 0xffffffff;
    m_pImage->m_colours[1] = 0xff000000;
    m_pImage->setPixmap(1U, 16);
    return true;
}

//-----------------------------------------------------------------------------
// HardwareTreeModel
//-----------------------------------------------------------------------------
HardwareTreeModel::HardwareTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

HardwareTreeModel::~HardwareTreeModel()
{
    delete rootItem;
}

int HardwareTreeModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

QVariant HardwareTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    HardwareBase *item = static_cast<HardwareBase*>(index.internalPointer());
    if (role == Qt::FontRole)
    {
        if (item->isHeader())
            return m_fontBold;

        if (index.column() == 1 && item->m_changed)
            return m_fontBold;

        return m_font;
    }

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case 0:
            return item->m_title;
        case 1:
            return item->m_text;
        }
    }
    return QVariant();
}

Qt::ItemFlags HardwareTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return QAbstractItemModel::flags(index);
}

QVariant HardwareTreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0:
            return QString("Name");
        case 1:
            return QString("Value");
        }
    }
    return QVariant();
}

QModelIndex HardwareTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    HardwareBase *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<HardwareBase*>(parent.internalPointer());

    HardwareBase *childItem = parentItem->m_children[row];
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex HardwareTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    HardwareBase *childItem = static_cast<HardwareBase*>(index.internalPointer());
    HardwareBase *parentItem = childItem->m_pParent;

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->m_rowIndex, 0, parentItem);
}

int HardwareTreeModel::rowCount(const QModelIndex &parent) const
{
    HardwareBase *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<HardwareBase*>(parent.internalPointer());

    return parentItem->m_children.size();
}

void HardwareTreeModel::dataChanged2(HardwareBase* pField)
{
    QModelIndex ind = createIndex(pField->m_rowIndex, 1, pField);
    emit dataChanged(ind, ind);
}

void HardwareTreeModel::UpdateSettings(const Session::Settings &settings)
{
    m_font = settings.m_font;
    m_fontBold = m_font;
    m_fontBold.setBold(true);
}

QModelIndex HardwareTreeModel::createIndex2(HardwareBase *pItem) const
{
    QModelIndex ind = createIndex(pItem->m_rowIndex, 0, pItem);
    return ind;
}

//-----------------------------------------------------------------------------
// HardwareWindow
//-----------------------------------------------------------------------------
HardwareWindow::HardwareWindow(QWidget *parent, Session* pSession) :
    QDockWidget(parent),
    m_pSession(pSession),
    m_pTargetModel(pSession->m_pTargetModel),
    m_pDispatcher(pSession->m_pDispatcher),
    m_videoMem(0, 0),
    m_mfpMem(0, 0)
{
    this->setWindowTitle("Hardware");
    setObjectName("Hardware");

    HardwareBase* m_pRoot = new HardwareBase();
    m_pModel = new HardwareTreeModel(this);
    m_pModel->rootItem = m_pRoot;

    HardwareHeader* pExpVec = new HardwareHeader("Vectors", "");
    HardwareHeader* pExpVecExceptions = new HardwareHeader("Exception Vectors", "");
    HardwareHeader* pExpVecAutos = new HardwareHeader("Auto-Vectors", "");
    HardwareHeader* pExpVecTraps = new HardwareHeader("Trap Vectors", "");
    HardwareHeader* pExpVecMfp = new HardwareHeader("MFP Vectors", "");

    HardwareHeader* pExpMmu = new HardwareHeader("MMU", "Memory Management Unit");
    HardwareHeader* pExpVideo = new HardwareHeader("Shifter/Glue", "Video");
    HardwareHeader* pExpMfp = new HardwareHeader("MFP 68901", "Multi-Function Peripheral");
    HardwareHeader* pExpYm = new HardwareHeader("YM/PSG", "Soundchip");
    HardwareHeader* pExpBlt = new HardwareHeader("Blitter", "");
    m_pRoot->AddChild(pExpVec);

    m_pRoot->AddChild(pExpMmu);
    m_pRoot->AddChild(pExpVideo);
    m_pRoot->AddChild(pExpMfp);
    m_pRoot->AddChild(pExpYm);
    m_pRoot->AddChild(pExpBlt);

    // ===== Vectors ====
    addShared(pExpVecExceptions, "Bus Error",           new HardwareFieldAddr(m_pSession, 0x8));
    addShared(pExpVecExceptions, "Address Eror",        new HardwareFieldAddr(m_pSession, 0xc));
    addShared(pExpVecExceptions, "Illegal Instruction", new HardwareFieldAddr(m_pSession, 0x10));
    addShared(pExpVecExceptions, "Zero Divide",         new HardwareFieldAddr(m_pSession, 0x14));
    addShared(pExpVecExceptions, "CHK/CHK2",            new HardwareFieldAddr(m_pSession, 0x18));
    addShared(pExpVecExceptions, "TRAPcc, TRAPV",       new HardwareFieldAddr(m_pSession, 0x1c));
    addShared(pExpVecExceptions, "Privilege Violation", new HardwareFieldAddr(m_pSession, 0x20));
    addShared(pExpVecExceptions, "Trace",               new HardwareFieldAddr(m_pSession, 0x24));
    addShared(pExpVecExceptions, "Line-A",              new HardwareFieldAddr(m_pSession, 0x28));
    addShared(pExpVecExceptions, "Line-F",              new HardwareFieldAddr(m_pSession, 0x2c));
    addShared(pExpVecExceptions, "Spurious Interrupt",  new HardwareFieldAddr(m_pSession, 0x60));
    pExpVec->AddChild(pExpVecExceptions);

    addShared(pExpVecAutos, "HBL",      new HardwareFieldAddr(m_pSession, 0x68));
    addShared(pExpVecAutos, "VBL",      new HardwareFieldAddr(m_pSession, 0x70));
    pExpVec->AddChild(pExpVecAutos);

    addShared(pExpVecTraps, "Trap #0",             new HardwareFieldAddr(m_pSession, 0x80));
    addShared(pExpVecTraps, "Trap #1 (GemDOS)",    new HardwareFieldAddr(m_pSession, 0x84));
    addShared(pExpVecTraps, "Trap #2 (AES/VDI)",   new HardwareFieldAddr(m_pSession, 0x88));
    addShared(pExpVecTraps, "Trap #3",             new HardwareFieldAddr(m_pSession, 0x8c));
    addShared(pExpVecTraps, "Trap #4",             new HardwareFieldAddr(m_pSession, 0x90));
    addShared(pExpVecTraps, "Trap #5",             new HardwareFieldAddr(m_pSession, 0x94));
    addShared(pExpVecTraps, "Trap #6",             new HardwareFieldAddr(m_pSession, 0x98));
    addShared(pExpVecTraps, "Trap #7",             new HardwareFieldAddr(m_pSession, 0x9c));
    addShared(pExpVecTraps, "Trap #8",             new HardwareFieldAddr(m_pSession, 0xa0));
    addShared(pExpVecTraps, "Trap #9",             new HardwareFieldAddr(m_pSession, 0xa4));
    addShared(pExpVecTraps, "Trap #10",            new HardwareFieldAddr(m_pSession, 0xa8));
    addShared(pExpVecTraps, "Trap #11",            new HardwareFieldAddr(m_pSession, 0xac));
    addShared(pExpVecTraps, "Trap #12",            new HardwareFieldAddr(m_pSession, 0xb0));
    addShared(pExpVecTraps, "Trap #13 (BIOS)",     new HardwareFieldAddr(m_pSession, 0xb4));
    addShared(pExpVecTraps, "Trap #14 (XBIOS)",    new HardwareFieldAddr(m_pSession, 0xb8));
    addShared(pExpVecTraps, "Trap #15",            new HardwareFieldAddr(m_pSession, 0xbc));
    pExpVec->AddChild(pExpVecTraps);

    addShared(pExpVecMfp, "Centronics busy",          new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 0));
    addShared(pExpVecMfp, "RS-232 DCD",               new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 1));
    addShared(pExpVecMfp, "RS-232 CTS",               new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 2));
    addShared(pExpVecMfp, "Blitter done",             new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 3));
    addShared(pExpVecMfp, "Timer D",                  new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 4));
    addShared(pExpVecMfp, "Timer C",                  new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 5));
    addShared(pExpVecMfp, "Keyboard/MIDI (ACIA)",     new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 6));
    addShared(pExpVecMfp, "FDC/HDC",                  new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 7));
    addShared(pExpVecMfp, "Timer B",                  new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 8));
    addShared(pExpVecMfp, "Send Error",               new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 9));
    addShared(pExpVecMfp, "Send buffer empty",        new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 10));
    addShared(pExpVecMfp, "Receive error",            new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 11));
    addShared(pExpVecMfp, "Receive buffer full",      new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 12));
    addShared(pExpVecMfp, "Timer A",                  new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 13));
    addShared(pExpVecMfp, "RS-232 Ring detect",       new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 14));
    addShared(pExpVecMfp, "GPI7 - Monochrome Detect", new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 15));
    pExpVec->AddChild(pExpVecMfp);

    // ===== MMU ====
    addField(pExpMmu,  "Bank 0",                   Regs::g_fieldDef_MMU_CONFIG_BANK0);
    addField(pExpMmu,  "Bank 1",                   Regs::g_fieldDef_MMU_CONFIG_BANK1);

    // ===== VIDEO ====
    addField(pExpVideo,  "Resolution",             Regs::g_fieldDef_VID_SHIFTER_RES_RES);
    addField(pExpVideo,  "Sync Rate",              Regs::g_fieldDef_VID_SYNC_MODE_RATE);
    addShared(pExpVideo, "Screen Base Address",    new HardwareFieldAddr(m_pSession, HardwareFieldAddr::ScreenBase));
    //addShared(pExpVideo, "Current Read Address",   new HardwareFieldAddr(m_pSession, HardwareFieldAddr::ScreenCurr));

    addField(pExpVideo, "Horizontal Scroll (STE)", Regs::g_fieldDef_VID_HORIZ_SCROLL_STE_PIXELS);
    addField(pExpVideo, "Scanline offset (STE)",   Regs::g_fieldDef_VID_SCANLINE_OFFSET_STE_ALL);

    // ===== MFP ====
    addField(pExpMfp, "Parallel Port Data",           Regs::g_fieldDef_MFP_GPIP_ALL);
    addMultiField(pExpMfp, "Active Edge low->high",   Regs::g_regFieldsDef_MFP_AER);
    addMultiField(pExpMfp, "Data Direction output",   Regs::g_regFieldsDef_MFP_DDR);

    addMultiField(pExpMfp, "IERA (Enable)",           Regs::g_regFieldsDef_MFP_IERA);
    addMultiField(pExpMfp, "IMRA (Mask)",             Regs::g_regFieldsDef_MFP_IMRA);
    addMultiField(pExpMfp, "IPRA (Pending)",          Regs::g_regFieldsDef_MFP_IPRA);
    addMultiField(pExpMfp, "ISRA (Service)",          Regs::g_regFieldsDef_MFP_ISRA);

    addMultiField(pExpMfp, "IERB (Enable)",           Regs::g_regFieldsDef_MFP_IERB);
    addMultiField(pExpMfp, "IMRB (Mask)",             Regs::g_regFieldsDef_MFP_IMRB);
    addMultiField(pExpMfp, "IPRB (Pending)",          Regs::g_regFieldsDef_MFP_IPRB);
    addMultiField(pExpMfp, "ISRB (Service)",          Regs::g_regFieldsDef_MFP_ISRB);

    addField(pExpMfp, "Vector Base offset",           Regs::g_fieldDef_MFP_VR_VEC_BASE_OFFSET);
    addField(pExpMfp, "End-of-Interrupt",             Regs::g_fieldDef_MFP_VR_ENDINT);


    addField(pExpMfp,  "Timer A Control Mode",         Regs::g_fieldDef_MFP_TACR_MODE_TIMER_A);
    addField(pExpMfp,  "Timer A Data",                 Regs::g_fieldDef_MFP_TADR_ALL);
    addShared(pExpMfp, "Timer A Vector",              new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 13));
    addField(pExpMfp,  "Timer B Control Mode",         Regs::g_fieldDef_MFP_TBCR_MODE_TIMER_B);
    addField(pExpMfp,  "Timer B Data",                 Regs::g_fieldDef_MFP_TBDR_ALL);
    addShared(pExpMfp, "Timer B Vector",              new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 8));
    addField(pExpMfp,  "Timer C Control Mode",         Regs::g_fieldDef_MFP_TCDCR_MODE_TIMER_C);
    addField(pExpMfp,  "Timer C Data",                 Regs::g_fieldDef_MFP_TCDR_ALL);
    addShared(pExpMfp, "Timer C Vector",              new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 5));
    addField(pExpMfp,  "Timer D Control Mode",         Regs::g_fieldDef_MFP_TCDCR_MODE_TIMER_D);
    addField(pExpMfp,  "Timer D Data",                 Regs::g_fieldDef_MFP_TDDR_ALL);
    addShared(pExpMfp, "Timer D Vector",              new HardwareFieldAddr(m_pSession, HardwareFieldAddr::Mfp, 4));

    addField(pExpMfp, "Sync Char",                    Regs::g_fieldDef_MFP_SCR_ALL);
    addMultiField(pExpMfp, "USART Control",           Regs::g_regFieldsDef_MFP_UCR);
    addMultiField(pExpMfp, "USART RX Status",         Regs::g_regFieldsDef_MFP_RSR);
    addMultiField(pExpMfp, "USART TX Status",         Regs::g_regFieldsDef_MFP_TSR);
    addField(pExpMfp, "USART Data",                   Regs::g_fieldDef_MFP_UDR_ALL);

    // ===== YM ====
    addShared(pExpYm, "Period A",     new HardwareFieldYmPeriod(Regs::YM_PERIOD_A_LO));
    addShared(pExpYm, "Period B",     new HardwareFieldYmPeriod(Regs::YM_PERIOD_B_LO));
    addShared(pExpYm, "Period C",     new HardwareFieldYmPeriod(Regs::YM_PERIOD_C_LO));
    addShared(pExpYm, "Noise Period", new HardwareFieldYmPeriod(Regs::YM_PERIOD_NOISE));
    addShared(pExpYm, "Mixer",        new HardwareFieldYmMixer());
    addShared(pExpYm, "Volume A",     new HardwareFieldYmVolume(Regs::YM_VOLUME_A));
    addShared(pExpYm, "Volume B",     new HardwareFieldYmVolume(Regs::YM_VOLUME_B));
    addShared(pExpYm, "Volume C",     new HardwareFieldYmVolume(Regs::YM_VOLUME_C));
    addShared(pExpYm, "Env Period",   new HardwareFieldYmPeriod(Regs::YM_PERIOD_ENV_LO));
    addShared(pExpYm, "Env Shape",    new HardwareFieldYmEnvShape());
    addShared(pExpYm, "Port A",       new HardwareFieldYm(Regs::YM_PORT_A));
    addShared(pExpYm, "Port B",       new HardwareFieldYm(Regs::YM_PORT_B));

    // ===== BLITTER ====
    // TODO these need sign expansion etc
   // addShared(pExpBlt, "Halftone RAM",     new HardwareBitmapBlitterHalftone(m_pSession));
    addField( pExpBlt, "Src X Inc",        Regs::g_fieldDef_BLT_SRC_INC_X_ALL);
    addField( pExpBlt, "Src Y Inc",        Regs::g_fieldDef_BLT_SRC_INC_Y_ALL);
    addShared(pExpBlt, "Src Addr",         new HardwareFieldAddr(m_pSession, HardwareFieldAddr::BltSrc));
    addField( pExpBlt, "Dst X Inc",        Regs::g_fieldDef_BLT_DST_INC_X_ALL);
    addField( pExpBlt, "Dst Y Inc",        Regs::g_fieldDef_BLT_DST_INC_Y_ALL);
    addShared(pExpBlt, "Dst Addr",         new HardwareFieldAddr(m_pSession, HardwareFieldAddr::BltDst));

    addField(pExpBlt, "X Count",           Regs::g_fieldDef_BLT_XCOUNT_ALL);
    addField(pExpBlt, "Y Count",           Regs::g_fieldDef_BLT_YCOUNT_ALL);

    addField(pExpBlt, "Halftone Op",       Regs::g_fieldDef_BLT_HALFTONE_OP_OP);
    addField(pExpBlt, "Logical Op",        Regs::g_fieldDef_BLT_LOGICAL_OP_OP);

    addMultiField(pExpBlt, "Control 1",    Regs::g_regFieldsDef_BLT_CTRL_1);
    addMultiField(pExpBlt, "Control 2",    Regs::g_regFieldsDef_BLT_CTRL_2);

    // Layouts
    QVBoxLayout* pMainLayout = new QVBoxLayout();
    SetMargins(pMainLayout);

    m_pView = new QTreeView(this);
    m_pView->setModel(m_pModel);

    m_pView->setExpanded(m_pModel->createIndex2(pExpVec), true);
    m_pView->setExpanded(m_pModel->createIndex2(pExpMmu), true);
    m_pView->setExpanded(m_pModel->createIndex2(pExpVideo), true);
    m_pView->setExpanded(m_pModel->createIndex2(pExpMfp), true);
    m_pView->setExpanded(m_pModel->createIndex2(pExpYm), true);
    m_pView->setExpanded(m_pModel->createIndex2(pExpBlt), true);

    pMainLayout->addWidget(m_pView);

    auto pMainRegion = new QWidget(this);   // whole panel
    pMainRegion->setLayout(pMainLayout);
    setWidget(pMainRegion);

    loadSettings();

    connect(m_pTargetModel,  &TargetModel::connectChangedSignal,   this, &HardwareWindow::connectChangedSlot);
    connect(m_pTargetModel,  &TargetModel::startStopChangedSignal, this, &HardwareWindow::startStopChangedSlot);
    connect(m_pTargetModel,  &TargetModel::memoryChangedSignal,    this, &HardwareWindow::memoryChangedSlot);
    connect(m_pTargetModel,  &TargetModel::ymChangedSignal,        this, &HardwareWindow::ymChangedSlot);

    connect(m_pSession,      &Session::settingsChanged,            this, &HardwareWindow::settingsChangedSlot);

    // Refresh enable state
    connectChangedSlot();

    // Refresh font
    settingsChangedSlot();

    // Call this after settingsChangedSlot so that the font is known
    m_pView->resizeColumnToContents(0);
}

HardwareWindow::~HardwareWindow()
{

}

void HardwareWindow::keyFocus()
{
    activateWindow();
    this->setFocus();
}

void HardwareWindow::loadSettings()
{
    QSettings settings;
    settings.beginGroup("Hardware");

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

//-----------------------------------------------------------------------------
void HardwareWindow::startStopChangedSlot()
{
    if (m_pTargetModel->IsRunning())
    {

    }
    else {
        m_pDispatcher->RequestMemory(MemorySlot::kHardwareWindow, Regs::MMU_CONFIG,    0x1);
        m_pDispatcher->RequestMemory(MemorySlot::kHardwareWindow, Regs::VID_REG_BASE,  0x70);
        m_pDispatcher->RequestMemory(MemorySlot::kHardwareWindow, Regs::MFP_GPIP,      0x30);
        m_pDispatcher->RequestMemory(MemorySlot::kHardwareWindow, Regs::BLT_HALFTONE_0,0x40);

        m_pDispatcher->InfoYm();
    }
}

void HardwareWindow::memoryChangedSlot(int memorySlot, uint64_t /*commandId*/)
{
    if (memorySlot == MemorySlot::kHardwareWindow)
    {
        const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kHardwareWindow);

        // Special case: MFP vector base register
        uint32_t vecBase;
        if (pMem && GetFieldVal(*pMem, Regs::g_fieldDef_MFP_VR_VEC_BASE_OFFSET, vecBase))
        {
            // We combine the upper 4 bits with the lower 4 bits of the MFP interrupt type
            uint32_t vecIndex = vecBase * 16;
            uint32_t vecAddr = vecIndex * 4;

            // Request memory
            m_pDispatcher->RequestMemory(MemorySlot::kHardwareWindowMfpVecs, vecAddr, 4 * 32);
        }
    }

    if (memorySlot == MemorySlot::kHardwareWindow || memorySlot == MemorySlot::kHardwareWindowMfpVecs)
    {
        for (auto pField : m_fields)
        {
            if (pField->Update(m_pTargetModel))
                m_pModel->dataChanged2(pField);
        }
    }
}

void HardwareWindow::ymChangedSlot()
{
    for (auto pField : m_fields)
    {
        pField->Update(m_pTargetModel);
    }
}

void HardwareWindow::settingsChangedSlot()
{
    for (auto pField : m_fields)
        pField->UpdateSettings(m_pSession->GetSettings());

    m_pModel->UpdateSettings(m_pSession->GetSettings());
}

void HardwareWindow::addField(HardwareBase* pLayout, const QString& title, const Regs::FieldDef &def)
{
    HardwareFieldRegEnum* pField = new HardwareFieldRegEnum(def);
    addShared(pLayout, title, pField);
}

void HardwareWindow::addMultiField(HardwareBase* pLayout, const QString& title, const Regs::FieldDef** defs)
{
    HardwareFieldMultiField* pField = new HardwareFieldMultiField(defs);
    addShared(pLayout, title, pField);
}

void HardwareWindow::addShared(HardwareBase *pLayout, const QString &title, HardwareField *pField)
{
    m_fields.append(pField);
    pField->m_title = title;
    pLayout->AddChild(pField);
}

