#include "graphicsinspector.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QCompleter>
#include <QSpinBox>
#include <QShortcut>
#include <QKeyEvent>
#include <QCheckBox>
#include <QComboBox>

#include <QFontDatabase>
#include <QSettings>
#include <QDebug>

#include "../transport/dispatcher.h"
#include "../models/targetmodel.h"
#include "../models/symboltablemodel.h"
#include "../models/stringparsers.h"
#include "../models/session.h"
#include "../hardware/regs_st.h"
#include "quicklayout.h"

/* A note on memory requests:
 * When doing update, the entry point is StartMemoryRequests()
 * This requests the video regs (whether needed or not)
 * When that memory is returned, a further memory request is made
 * by RequestBitmapMemory.
 * Once that memory is returned the UI's bitmap is created/updated.
 */

static void CreateBitplanePalette(QVector<uint32_t>& palette,
                                  uint32_t col0,
                                  uint32_t col1,
                                  uint32_t col2,
                                  uint32_t col3)
{
    palette.append(0xff000000 + 0                  );
    palette.append(0xff000000 +               +col0);
    palette.append(0xff000000 +          +col1     );
    palette.append(0xff000000 +          +col1+col0);
    palette.append(0xff000000 +      col2          );
    palette.append(0xff000000 +      col2     +col0);
    palette.append(0xff000000 +      col2+col1     );
    palette.append(0xff000000 +      col2+col1+col0);
    palette.append(0xff000000 + col3               );
    palette.append(0xff000000 + col3          +col0);
    palette.append(0xff000000 + col3     +col1     );
    palette.append(0xff000000 + col3     +col1+col0);
    palette.append(0xff000000 + col3+col2          );
    palette.append(0xff000000 + col3+col2     +col0);
    palette.append(0xff000000 + col3+col2+col1     );
    palette.append(0xff000000 + col3+col2+col1+col0);
}

GraphicsInspectorWidget::GraphicsInspectorWidget(QWidget *parent,
                                                 Session* pSession) :
    QDockWidget(parent),
    m_pSession(pSession),
    m_mode(k4Bitplane),
    m_address(0U),
    m_width(20),
    m_height(200),
    m_padding(0),
    m_requestIdBitmap(0U),
    m_requestIdVideoRegs(0U)
{
    m_pTargetModel = m_pSession->m_pTargetModel;
    m_pDispatcher = m_pSession->m_pDispatcher;

    QString name("GraphicsInspector");
    this->setObjectName(name);
    this->setWindowTitle(name);
    this->setFocusPolicy(Qt::FocusPolicy::StrongFocus);

    // Make img widget first so that tab order "works"
    m_pImageWidget = new NonAntiAliasImage(this, pSession);

    // top line
    m_pAddressLineEdit = new QLineEdit(this);
    m_pLockAddressToVideoCheckBox = new QCheckBox(tr("Use Registers"), this);
    m_pSymbolTableModel = new SymbolTableModel(this, m_pTargetModel->GetSymbolTable());
    QCompleter* pCompl = new QCompleter(m_pSymbolTableModel, this);
    pCompl->setCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
    m_pAddressLineEdit->setCompleter(pCompl);

    // Second line
    m_pModeComboBox = new QComboBox(this);
    m_pWidthSpinBox = new QSpinBox(this);
    m_pHeightSpinBox = new QSpinBox(this);
    m_pPaddingSpinBox = new QSpinBox(this);
    m_pLockFormatToVideoCheckBox = new QCheckBox(tr("Use Registers"), this);

    m_pModeComboBox->addItem(tr("4 Plane"), Mode::k4Bitplane);
    m_pModeComboBox->addItem(tr("2 Plane"), Mode::k2Bitplane);
    m_pModeComboBox->addItem(tr("1 Plane"), Mode::k1Bitplane);
    m_pWidthSpinBox->setRange(1, 40);
    m_pWidthSpinBox->setValue(m_width);
    m_pHeightSpinBox->setRange(16, 256);
    m_pHeightSpinBox->setValue(m_height);
    m_pPaddingSpinBox->setRange(0, 8);
    m_pPaddingSpinBox->setValue(m_padding);

    // Third line
    m_pPaletteComboBox = new QComboBox(this);
    m_pPaletteComboBox->addItem(tr("Greyscale"), kGreyscale);
    m_pPaletteComboBox->addItem(tr("Contrast1"), kContrast1);
    m_pPaletteComboBox->addItem(tr("Bitplane0"), kBitplane0);
    m_pPaletteComboBox->addItem(tr("Bitplane1"), kBitplane1);
    m_pPaletteComboBox->addItem(tr("Bitplane2"), kBitplane2);
    m_pPaletteComboBox->addItem(tr("Bitplane3"), kBitplane3);

    m_pLockPaletteToVideoCheckBox = new QCheckBox(tr("Use Registers"), this);
    m_pInfoLabel = new QLabel(this);

    // Bottom
    m_pImageWidget->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));

    // Layout First line
    QHBoxLayout *hlayout1 = new QHBoxLayout();
    SetMargins(hlayout1);
    hlayout1->setAlignment(Qt::AlignLeft);
    hlayout1->addWidget(new QLabel(tr("Address:"), this));
    hlayout1->addWidget(m_pAddressLineEdit);
    hlayout1->addWidget(m_pLockAddressToVideoCheckBox);
    QWidget* pContainer1 = new QWidget(this);
    pContainer1->setLayout(hlayout1);

    // Layout Second line
    QHBoxLayout *hlayout2 = new QHBoxLayout();
    SetMargins(hlayout2);
    hlayout2->setAlignment(Qt::AlignLeft);
    hlayout2->addWidget(new QLabel(tr("Format:"), this));
    hlayout2->addWidget(m_pModeComboBox);
    hlayout2->addWidget(new QLabel(tr("Width:"), this));
    hlayout2->addWidget(m_pWidthSpinBox);
    hlayout2->addWidget(new QLabel(tr("Height:"), this));
    hlayout2->addWidget(m_pHeightSpinBox);
    hlayout2->addWidget(new QLabel(tr("Pad:"), this));
    hlayout2->addWidget(m_pPaddingSpinBox);

    hlayout2->addWidget(m_pLockFormatToVideoCheckBox);
    QWidget* pContainer2 = new QWidget(this);
    pContainer2->setLayout(hlayout2);

    // Layout Third line
    QHBoxLayout *hlayout3 = new QHBoxLayout();
    SetMargins(hlayout3);
    hlayout3->setAlignment(Qt::AlignLeft);
    hlayout3->addWidget(new QLabel(tr("Palette:"), this));
    hlayout3->addWidget(m_pPaletteComboBox);
    hlayout3->addWidget(m_pLockPaletteToVideoCheckBox);
    hlayout3->addWidget(m_pInfoLabel);
    QWidget* pContainer3 = new QWidget(this);
    pContainer3->setLayout(hlayout3);

    auto pMainGroupBox = new QWidget(this);
    QVBoxLayout *vlayout = new QVBoxLayout;
    SetMargins(vlayout);
    vlayout->addWidget(pContainer1);
    vlayout->addWidget(pContainer2);
    vlayout->addWidget(pContainer3);
    vlayout->addWidget(m_pImageWidget);
    vlayout->setAlignment(Qt::Alignment(Qt::AlignTop));
    pMainGroupBox->setLayout(vlayout);

    setWidget(pMainGroupBox);
    m_pLockAddressToVideoCheckBox->setChecked(true);
    m_pLockFormatToVideoCheckBox->setChecked(true);

    connect(m_pTargetModel,  &TargetModel::connectChangedSignal,          this, &GraphicsInspectorWidget::connectChangedSlot);
    connect(m_pTargetModel,  &TargetModel::startStopChangedSignal,        this, &GraphicsInspectorWidget::startStopChangedSlot);
    connect(m_pTargetModel,  &TargetModel::startStopChangedSignalDelayed, this, &GraphicsInspectorWidget::startStopDelayedChangedSlot);
    connect(m_pTargetModel,  &TargetModel::memoryChangedSignal,           this, &GraphicsInspectorWidget::memoryChangedSlot);
    connect(m_pTargetModel,  &TargetModel::otherMemoryChangedSignal,      this, &GraphicsInspectorWidget::otherMemoryChangedSlot);
    connect(m_pTargetModel,  &TargetModel::runningRefreshTimerSignal,     this, &GraphicsInspectorWidget::runningRefreshTimerSlot);

    connect(m_pAddressLineEdit,             &QLineEdit::returnPressed,    this, &GraphicsInspectorWidget::textEditChangedSlot);
    connect(m_pLockAddressToVideoCheckBox,  &QCheckBox::stateChanged,     this, &GraphicsInspectorWidget::lockAddressToVideoChangedSlot);
    connect(m_pLockFormatToVideoCheckBox,   &QCheckBox::stateChanged,     this, &GraphicsInspectorWidget::lockFormatToVideoChangedSlot);
    connect(m_pLockAddressToVideoCheckBox,  &QCheckBox::stateChanged,     this, &GraphicsInspectorWidget::lockAddressToVideoChangedSlot);
    connect(m_pLockPaletteToVideoCheckBox,  &QCheckBox::stateChanged,     this, &GraphicsInspectorWidget::lockPaletteToVideoChangedSlot);

    connect(m_pModeComboBox,    SIGNAL(currentIndexChanged(int)),         SLOT(modeChangedSlot(int)));
    connect(m_pPaletteComboBox, SIGNAL(currentIndexChanged(int)),         SLOT(paletteChangedSlot(int)));
    connect(m_pWidthSpinBox,    SIGNAL(valueChanged(int)),                SLOT(widthChangedSlot(int)));
    connect(m_pHeightSpinBox,   SIGNAL(valueChanged(int)),                SLOT(heightChangedSlot(int)));
    connect(m_pPaddingSpinBox,  SIGNAL(valueChanged(int)),                SLOT(paddingChangedSlot(int)));

    connect(m_pImageWidget,  &NonAntiAliasImage::StringChanged,           this, &GraphicsInspectorWidget::tooltipStringChangedSlot);

    connect(m_pSession,      &Session::addressRequested,                  this, &GraphicsInspectorWidget::requestAddress);

    loadSettings();
    UpdateUIElements();
    DisplayAddress();
}

GraphicsInspectorWidget::~GraphicsInspectorWidget()
{

}

void GraphicsInspectorWidget::keyFocus()
{
    activateWindow();
    m_pImageWidget->setFocus();
}

void GraphicsInspectorWidget::loadSettings()
{
    QSettings settings;
    settings.beginGroup("GraphicsInspector");

    restoreGeometry(settings.value("geometry").toByteArray());
    m_width = settings.value("width", QVariant(20)).toInt();
    m_height = settings.value("height", QVariant(200)).toInt();
    m_padding = settings.value("padding", QVariant(0)).toInt();
    m_mode = static_cast<Mode>(settings.value("mode", QVariant((int)Mode::k4Bitplane)).toInt());
    m_pModeComboBox->setCurrentIndex(m_mode);

    m_pWidthSpinBox->setValue(m_width);
    m_pHeightSpinBox->setValue(m_height);
    m_pPaddingSpinBox->setValue(m_padding);
    m_pLockAddressToVideoCheckBox->setChecked(settings.value("lockAddress", QVariant(true)).toBool());
    m_pLockFormatToVideoCheckBox->setChecked(settings.value("lockFormat", QVariant(true)).toBool());
    m_pLockPaletteToVideoCheckBox->setChecked(settings.value("lockPalette", QVariant(true)).toBool());

    int palette = settings.value("palette", QVariant((int)Palette::kGreyscale)).toInt();
    m_pPaletteComboBox->setCurrentIndex(palette);
    settings.endGroup();
}

void GraphicsInspectorWidget::saveSettings()
{
    QSettings settings;
    settings.beginGroup("GraphicsInspector");

    settings.setValue("geometry", saveGeometry());
    settings.setValue("width", m_width);
    settings.setValue("height", m_height);
    settings.setValue("padding", m_padding);
    settings.setValue("lockAddress", m_pLockAddressToVideoCheckBox->isChecked());
    settings.setValue("lockFormat", m_pLockFormatToVideoCheckBox->isChecked());
    settings.setValue("lockPalette", m_pLockPaletteToVideoCheckBox->isChecked());
    settings.setValue("mode", static_cast<int>(m_mode));
    settings.setValue("palette", m_pPaletteComboBox->currentIndex());
    settings.endGroup();
}

void GraphicsInspectorWidget::keyPressEvent(QKeyEvent* ev)
{
    int offset = 0;

    EffectiveData data;
    GetEffectiveData(data);

    int32_t height = GetEffectiveHeight();

    bool shift = (ev->modifiers().testFlag(Qt::KeyboardModifier::ShiftModifier));

    if (ev->key() == Qt::Key::Key_Up)
        offset = shift ? -8 * data.bytesPerLine : -data.bytesPerLine;
    else if (ev->key() == Qt::Key::Key_Down)
        offset = shift ? 8 * data.bytesPerLine : data.bytesPerLine;
    else if (ev->key() == Qt::Key::Key_PageUp)
        offset = -height * data.bytesPerLine;
    else if (ev->key() == Qt::Key::Key_PageDown)
        offset = +height * data.bytesPerLine;
    else if (ev->key() == Qt::Key::Key_Left)
        offset = -2;
    else if (ev->key() == Qt::Key::Key_Right)
        offset = 2;

    if (offset && m_requestIdBitmap == 0)
    {
        // Going up or down by a small amount is OK
        if (offset > 0 || m_address > -offset)
        {
            m_address += offset;
        }
        else {
            m_address = 0;
        }
        m_pLockAddressToVideoCheckBox->setChecked(false);
        RequestBitmapMemory();
        DisplayAddress();

        return;
    }
    QDockWidget::keyPressEvent(ev);

}

void GraphicsInspectorWidget::connectChangedSlot()
{
    if (!m_pTargetModel->IsConnected())
    {
        m_pImageWidget->setPixmap(0, 0);
    }
}

void GraphicsInspectorWidget::startStopChangedSlot()
{
    // Nothing happens here except an initial redraw
    update();
}

void GraphicsInspectorWidget::startStopDelayedChangedSlot()
{
    // Request new memory for the view
    if (!m_pTargetModel->IsRunning())
    {
        StartMemoryRequests();
        // Don't clear the Running flag for the widget, do that when the memory arrives
    }
    else {
        // Turn on the running mask slowly
        m_pImageWidget->SetRunning(true);
    }
}

void GraphicsInspectorWidget::memoryChangedSlot(int /*memorySlot*/, uint64_t commandId)
{
    if (commandId == m_requestIdVideoRegs)
    {
        // This is the first stage of the request process for a bitmap
        const Memory* pMemOrig = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
        if (!pMemOrig)
            return;

        UpdateFormatFromSettings();
        UpdateUIElements();
        m_requestIdVideoRegs = 0;

        // We should have registers now, so use them
        if (m_pLockAddressToVideoCheckBox->isChecked())
            SetAddressFromVideo();

        RequestBitmapMemory();
    }
    else if (commandId == m_requestIdBitmap)
    {
        const Memory* pMemOrig = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspector);
        if (!pMemOrig)
            return;

        EffectiveData data;
        GetEffectiveData(data);

        Mode mode = data.mode;
        int width = data.width;
        int height = data.height;

        // Uncompress
        int required = data.requiredSize;

        // Ensure we have the right size memory
        if (pMemOrig->GetSize() < required)
            return;

        int bitmapSize = width * 16 * height;
        uint8_t* pDestPixels = m_pImageWidget->AllocBitmap(bitmapSize);

        if (mode == k4Bitplane)
        {
            for (int y = 0; y < height; ++y)
            {
                const uint8_t* pChunk = pMemOrig->GetData() + y * data.bytesPerLine;
                for (int x = 0; x < width; ++x)
                {
                    int32_t pSrc[4];    // top 16 bits never used
                    pSrc[3] = (pChunk[0] << 8) | pChunk[1];
                    pSrc[2] = (pChunk[2] << 8) | pChunk[3];
                    pSrc[1] = (pChunk[4] << 8) | pChunk[5];
                    pSrc[0] = (pChunk[6] << 8) | pChunk[7];
                    for (int pix = 15; pix >= 0; --pix)
                    {
                        uint8_t val;
                        val  = (pSrc[0] & 1); val <<= 1;
                        val |= (pSrc[1] & 1); val <<= 1;
                        val |= (pSrc[2] & 1); val <<= 1;
                        val |= (pSrc[3] & 1);

                        pDestPixels[pix] = val;
                        pSrc[0] >>= 1;
                        pSrc[1] >>= 1;
                        pSrc[2] >>= 1;
                        pSrc[3] >>= 1;
                    }
                    pChunk += 8;
                    pDestPixels += 16;
                }

            }

        }
        else if (mode == k2Bitplane)
        {
            for (int y = 0; y < height; ++y)
            {
                const uint8_t* pChunk = pMemOrig->GetData() + y * data.bytesPerLine;
                for (int x = 0; x < width; ++x)
                {
                    int32_t pSrc[2];
                    pSrc[1] = (pChunk[0] << 8) | pChunk[1];
                    pSrc[0] = (pChunk[2] << 8) | pChunk[3];
                    for (int pix = 15; pix >= 0; --pix)
                    {
                        uint8_t val;
                        val  = (pSrc[0] & 1); val <<= 1;
                        val |= (pSrc[1] & 1);
                        pDestPixels[pix] = val;
                        pSrc[0] >>= 1;
                        pSrc[1] >>= 1;
                    }
                    pChunk += 4;
                    pDestPixels += 16;
                }
            }
        }
        else if (mode == k1Bitplane)
        {
            for (int y = 0; y < height; ++y)
            {
                const uint8_t* pChunk = pMemOrig->GetData() + y * data.bytesPerLine;
                for (int x = 0; x < width; ++x)
                {
                    int32_t pSrc[1];
                    pSrc[0] = (pChunk[0] << 8) | pChunk[1];
                    for (int pix = 15; pix >= 0; --pix)
                    {
                        uint8_t val;
                        val  = (pSrc[0] & 1);
                        pDestPixels[pix] = val;
                        pSrc[0] >>= 1;
                    }
                    pChunk += 2;
                    pDestPixels += 16;
                }
            }
        }

        // Update image in the widget
        UpdatePaletteFromSettings();

        // Clear the running mask, but only if we're really stopped
        if (!m_pTargetModel->IsRunning())
            m_pImageWidget->SetRunning(false);

        m_pImageWidget->setPixmap(width, height);

        m_requestIdBitmap = 0;
        return;
    }
}

void GraphicsInspectorWidget::textEditChangedSlot()
{
    std::string expression = m_pAddressLineEdit->text().toStdString();
    uint32_t addr;
    if (!StringParsers::ParseExpression(expression.c_str(), addr,
                                        m_pTargetModel->GetSymbolTable(),
                                        m_pTargetModel->GetRegs()))
    {
        return;
    }
    m_address = addr;
    m_pLockAddressToVideoCheckBox->setChecked(false);
    UpdateUIElements();
    RequestBitmapMemory();
}

void GraphicsInspectorWidget::lockAddressToVideoChangedSlot()
{
    bool m_bLockToVideo = m_pLockAddressToVideoCheckBox->isChecked();
    if (m_bLockToVideo)
    {
        SetAddressFromVideo();
        RequestBitmapMemory();
    }
    UpdateUIElements();
}

void GraphicsInspectorWidget::lockFormatToVideoChangedSlot()
{
    bool m_bLockToVideo = m_pLockFormatToVideoCheckBox->isChecked();
    if (m_bLockToVideo)
    {
        UpdateFormatFromSettings();
        RequestBitmapMemory();
    }
    UpdateUIElements();
}

void GraphicsInspectorWidget::lockPaletteToVideoChangedSlot()
{
    UpdatePaletteFromSettings();

    // Ensure pixmap is updated
    m_pImageWidget->setPixmap(GetEffectiveWidth(), GetEffectiveHeight());

    UpdateUIElements();
}

void GraphicsInspectorWidget::modeChangedSlot(int index)
{
    m_mode = static_cast<Mode>(index);
    RequestBitmapMemory();
}

void GraphicsInspectorWidget::paletteChangedSlot(int index)
{
    UpdatePaletteFromSettings();
    // Ensure pixmap is updated
    m_pImageWidget->setPixmap(GetEffectiveWidth(), GetEffectiveHeight());

    UpdateUIElements();
}

void GraphicsInspectorWidget::otherMemoryChangedSlot(uint32_t address, uint32_t size)
{
    // Do a re-request if our memory is touched
    uint32_t ourSize = GetEffectiveHeight() * GetEffectiveWidth() * BytesPerMode(m_mode);
    if (Overlaps(m_address, ourSize, address, size))
        RequestBitmapMemory();
}

void GraphicsInspectorWidget::runningRefreshTimerSlot()
{
    //if (m_pSession->GetSettings().m_liveRefresh)
    //    StartMemoryRequests();
}

void GraphicsInspectorWidget::widthChangedSlot(int value)
{
    m_width = value;
    RequestBitmapMemory();
}

void GraphicsInspectorWidget::heightChangedSlot(int value)
{
    m_height = value;
    RequestBitmapMemory();
}

void GraphicsInspectorWidget::paddingChangedSlot(int value)
{
    m_padding = value;
    RequestBitmapMemory();
}

void GraphicsInspectorWidget::tooltipStringChangedSlot()
{
    m_pInfoLabel->setText(m_pImageWidget->GetString());
}

void GraphicsInspectorWidget::requestAddress(Session::WindowType type, int windowIndex, uint32_t address)
{
    if (type != Session::WindowType::kGraphicsInspector)
        return;

    (void) windowIndex;

    // Set the address and override video regs
    m_address = address;
    m_pLockAddressToVideoCheckBox->setChecked(false);
    UpdateUIElements();
    RequestBitmapMemory();
}

void GraphicsInspectorWidget::StartMemoryRequests()
{
    m_requestIdVideoRegs = 0;

    // This is the main entry point for grabbing the data.
    // Trigger a full refresh of registers
    if (!m_pTargetModel->IsConnected())
        return;

    m_requestIdVideoRegs = m_pDispatcher->ReadMemory(MemorySlot::kGraphicsInspectorVideoRegs, Regs::VID_REG_BASE, 0x70);
}


// Request enough memory based on m_rowCount and m_logicalAddr
void GraphicsInspectorWidget::RequestBitmapMemory()
{
    m_requestIdBitmap = 0;
    if (!m_pTargetModel->IsConnected())
        return;

    // Video data first. Once that has returned we request the necessary amount of memory
    // Request video memory area
    EffectiveData data;
    GetEffectiveData(data);
    m_requestIdBitmap = m_pDispatcher->ReadMemory(MemorySlot::kGraphicsInspector, m_address, data.requiredSize);
    DisplayAddress();
}

bool GraphicsInspectorWidget::SetAddressFromVideo()
{
    // Update to current video regs
    const Memory* pVideoRegs = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
    if (!pVideoRegs)
        return false;

    uint32_t address;
    if (HardwareST::GetVideoBase(*pVideoRegs, m_pTargetModel->GetMachineType(), address))
    {
        m_address = address;
        return true;
    }
    return false;
}

void GraphicsInspectorWidget::DisplayAddress()
{
    m_pAddressLineEdit->setText(QString::asprintf("$%x", m_address));
}

void GraphicsInspectorWidget::UpdatePaletteFromSettings()
{
    // Colours are ARGB
    m_pImageWidget->m_colours.clear();

    // Now update palette
    if (m_pLockPaletteToVideoCheckBox->isChecked())
    {
        const Memory* pMemOrig = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
        if (!pMemOrig)
            return;
        static const uint32_t stToRgb[16] =
        {
            0x00, 0x22, 0x44, 0x66, 0x88, 0xaa, 0xcc, 0xee,
            0x00, 0x22, 0x44, 0x66, 0x88, 0xaa, 0xcc, 0xee
        };
        static const uint32_t steToRgb[16] =
        {
            0x00, 0x22, 0x44, 0x66, 0x88, 0xaa, 0xcc, 0xee,
            0x11, 0x33, 0x55, 0x77, 0x99, 0xbb, 0xdd, 0xff
        };

        bool isST = IsMachineST(m_pTargetModel->GetMachineType());
        const uint32_t* pPalette = isST ? stToRgb : steToRgb;


        for (uint i = 0; i < 16; ++i)
        {
            uint32_t addr = Regs::VID_PAL_0 + i*2;
            uint32_t  r = pMemOrig->ReadAddressByte(addr + 0) & 0xf;
            uint32_t  g = pMemOrig->ReadAddressByte(addr + 1) >> 4;
            uint32_t  b = pMemOrig->ReadAddressByte(addr + 1) & 0xf;

            uint32_t colour = 0xff000000U;
            colour |= pPalette[r] << 16;
            colour |= pPalette[g] << 8;
            colour |= pPalette[b] << 0;
            m_pImageWidget->m_colours.append(colour);
        }
    }
    else {
        switch (m_pPaletteComboBox->currentIndex())
        {
        case kGreyscale:
            for (uint i = 0; i < 16; ++i)
            {
                m_pImageWidget->m_colours.append(0xff000000 + i * 0x101010);
            }
            break;
        case kContrast1:
            // This palette is derived from one of the bitplane palettes in "44"
            CreateBitplanePalette(m_pImageWidget->m_colours, 0x500000*2, 0x224400*2, 0x003322*2, 0x000055*2);
            break;
        case kBitplane0:
            CreateBitplanePalette(m_pImageWidget->m_colours, 0xbbbbbb, 0x220000, 0x2200, 0x22);
            break;
        case kBitplane1:
            CreateBitplanePalette(m_pImageWidget->m_colours, 0x220000, 0xbbbbbb, 0x2200, 0x22);
            break;
        case kBitplane2:
            CreateBitplanePalette(m_pImageWidget->m_colours, 0x220000, 0x2200, 0xbbbbbb, 0x22);
            break;
        case kBitplane3:
            CreateBitplanePalette(m_pImageWidget->m_colours, 0x220000, 0x2200, 0x22, 0xbbbbbb);
            break;
        default:
            break;
        }
    }
}

void GraphicsInspectorWidget::UpdateFormatFromSettings()
{
    // Now we have the registers, we can now video dimensions.
    int width = GetEffectiveWidth();
    int height = GetEffectiveHeight();
    Mode mode = GetEffectiveMode();

    // Set these as current values, so that if we scroll around,
    // they are not lost
    m_width = width;
    m_height = height;
    m_mode = mode;
    UpdateUIElements();
}

void GraphicsInspectorWidget::UpdateUIElements()
{
    m_pWidthSpinBox->setValue(m_width);
    m_pHeightSpinBox->setValue(m_height);
    m_pPaddingSpinBox->setValue(m_padding);
    m_pModeComboBox->setCurrentIndex(m_mode);

    m_pWidthSpinBox->setEnabled(!m_pLockFormatToVideoCheckBox->isChecked());
    m_pHeightSpinBox->setEnabled(!m_pLockFormatToVideoCheckBox->isChecked());
    m_pPaddingSpinBox->setEnabled(!m_pLockFormatToVideoCheckBox->isChecked());
    m_pModeComboBox->setEnabled(!m_pLockFormatToVideoCheckBox->isChecked());

    m_pPaletteComboBox->setEnabled(!m_pLockPaletteToVideoCheckBox->isChecked());
    DisplayAddress();
}

GraphicsInspectorWidget::Mode GraphicsInspectorWidget::GetEffectiveMode() const
{
    if (!m_pLockFormatToVideoCheckBox->isChecked())
        return m_mode;

    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
    if (!pMem)
        return Mode::k4Bitplane;
    Regs::RESOLUTION modeReg = Regs::GetField_VID_SHIFTER_RES_RES(pMem->ReadAddressByte(Regs::VID_SHIFTER_RES));
    if (modeReg == Regs::RESOLUTION::LOW)
        return Mode::k4Bitplane;
    else if (modeReg == Regs::RESOLUTION::MEDIUM)
        return Mode::k2Bitplane;

    return Mode::k1Bitplane;
}

int GraphicsInspectorWidget::GetEffectiveWidth() const
{
    if (!m_pLockFormatToVideoCheckBox->isChecked())
        return m_width;

    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
    if (!pMem)
        return Mode::k4Bitplane;

    // Handle ST scroll
    int width = 0;
    if (!IsMachineST(m_pTargetModel->GetMachineType()))
    {
        uint8_t modeReg = Regs::GetField_VID_HORIZ_SCROLL_STE_PIXELS(pMem->ReadAddressByte(Regs::VID_HORIZ_SCROLL_STE));
        if (modeReg != 0)
            width = 1;  // extra read for scroll
    }

    Regs::RESOLUTION modeReg = Regs::GetField_VID_SHIFTER_RES_RES(pMem->ReadAddressByte(Regs::VID_SHIFTER_RES));
    if (modeReg == Regs::RESOLUTION::LOW)
        return width + 20;
    else if (modeReg == Regs::RESOLUTION::MEDIUM)
        return width + 40;
    return width + 40;
}

int GraphicsInspectorWidget::GetEffectiveHeight() const
{
    if (!m_pLockFormatToVideoCheckBox->isChecked())
        return m_height;

    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
    if (!pMem)
        return Mode::k4Bitplane;
    Regs::RESOLUTION modeReg = Regs::GetField_VID_SHIFTER_RES_RES(pMem->ReadAddressByte(Regs::VID_SHIFTER_RES));
    if (modeReg == Regs::RESOLUTION::LOW)
        return 200;
    else if (modeReg == Regs::RESOLUTION::MEDIUM)
        return 200;
    return 400;
}

int GraphicsInspectorWidget::GetEffectivePadding() const
{
    if (!m_pLockFormatToVideoCheckBox->isChecked())
        return m_padding;

    return 0;       // TODO handle STE padding
}

void GraphicsInspectorWidget::GetEffectiveData(GraphicsInspectorWidget::EffectiveData &data) const
{
    data.mode = GetEffectiveMode();
    data.width = GetEffectiveWidth();
    data.height = GetEffectiveHeight();
    data.bytesPerLine = data.width * BytesPerMode(data.mode) + GetEffectivePadding();
    data.requiredSize = data.bytesPerLine * data.height;
}

int32_t GraphicsInspectorWidget::BytesPerMode(GraphicsInspectorWidget::Mode mode)
{
    switch (mode)
    {
    case k4Bitplane: return 8;
    case k2Bitplane: return 4;
    case k1Bitplane: return 2;
    }
    return 0;
}
