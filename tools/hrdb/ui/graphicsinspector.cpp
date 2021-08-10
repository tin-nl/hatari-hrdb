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

#include <QPainter>
#include <QStyle>
#include <QFontDatabase>
#include <QSettings>
#include <QDebug>

#include "../transport/dispatcher.h"
#include "../models/targetmodel.h"
#include "../models/symboltablemodel.h"
#include "../models/stringparsers.h"
#include "../hardware/hardware_st.h"
#include "quicklayout.h"

NonAntiAliasImage::NonAntiAliasImage(QWidget *parent)
    : QWidget(parent),
      m_pBitmap(nullptr),
      m_bitmapSize(0)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
}

void NonAntiAliasImage::setPixmap(int width, int height)
{
    QImage img(m_pBitmap, width * 16, height, QImage::Format_Indexed8);
    img.setColorTable(m_colours);
    QPixmap pm = QPixmap::fromImage(img);
    m_pixmap = pm;
    UpdateString();
    emit StringChanged();
    update();
}

NonAntiAliasImage::~NonAntiAliasImage()
{
    delete [] m_pBitmap;
}

uint8_t* NonAntiAliasImage::AllocBitmap(int size)
{
    if (size == m_bitmapSize)
        return m_pBitmap;

    delete [] m_pBitmap;
    m_pBitmap = new uint8_t[size];
    m_bitmapSize = size;
    return m_pBitmap;
}

void NonAntiAliasImage::paintEvent(QPaintEvent* ev)
{
    QPainter painter(this);

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (m_pixmap.width() != 0)
    {
        const QRect& r = rect();
        painter.setRenderHint(QPainter::Antialiasing, false);
        style()->drawItemPixmap(&painter, r, Qt::AlignCenter, m_pixmap.scaled(r.size()));
    }
    else {
        painter.setFont(monoFont);
        painter.drawText(10, 10, "Not connected");
    }
    const QPalette& pal = this->palette();

    painter.setPen(QPen(pal.dark(), hasFocus() ? 6 : 2));
    painter.drawRect(this->rect());

    QWidget::paintEvent(ev);
}

void NonAntiAliasImage::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePos = event->localPos();
    if (this->underMouse())
    {
        UpdateString();
        emit StringChanged();
    }
    QWidget::mouseMoveEvent(event);
}

void NonAntiAliasImage::UpdateString()
{
    m_infoString.clear();
    if (m_pixmap.width() == 0)
        return;

    // We need to work out the dimensions here
    if (this->underMouse())
    {
        const QRect& r = rect();
        double x_frac = (m_mousePos.x() - r.x()) / r.width();
        double y_frac = (m_mousePos.y() - r.y()) / r.height();

        double x_pix = x_frac * m_pixmap.width();
        double y_pix = y_frac * m_pixmap.height();

        int x = static_cast<int>(x_pix);
        int y = static_cast<int>(y_pix);
        m_infoString = QString::asprintf("X:%d Y:%d", x, y);

        if (x < m_pixmap.width() && y < m_pixmap.height() &&
            m_pBitmap)
        {
            uint8_t pixelValue = m_pBitmap[y * m_pixmap.width() + x];
            m_infoString += QString::asprintf(" Pixel value: %u", pixelValue);
        }

        qDebug() << m_infoString;
    }
}

GraphicsInspectorWidget::GraphicsInspectorWidget(QWidget *parent,
                                                 TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDockWidget(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher),
    m_mode(k4Bitplane),
    m_address(0U),
    m_width(20),
    m_height(200),
    m_requestIdBitmap(0U),
    m_requestIdVideoRegs(0U)
{
    QString name("GraphicsInspector");
    this->setObjectName(name);
    this->setWindowTitle(name);
    this->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    m_pLockToVideoCheckBox = new QCheckBox(tr("Lock to Video Regs"), this);
    m_pInfoLabel = new QLabel(this);

    // Do these in tab order
    m_pLineEdit = new QLineEdit(this);
    m_pModeComboBox = new QComboBox(this);
    m_pWidthSpinBox = new QSpinBox(this);
    m_pHeightSpinBox = new QSpinBox(this);

    m_pImageWidget = new NonAntiAliasImage(this);
    m_pImageWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    //m_pPictureLabel->setFixedSize(640, 400);
    //m_pPictureLabel->setScaledContents(true);

    m_pSymbolTableModel = new SymbolTableModel(this, m_pTargetModel->GetSymbolTable());
    QCompleter* pCompl = new QCompleter(m_pSymbolTableModel, this);
    pCompl->setCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
    m_pLineEdit->setCompleter(pCompl);

    m_pModeComboBox->addItem(tr("4 Plane"), Mode::k4Bitplane);
    m_pModeComboBox->addItem(tr("2 Plane"), Mode::k2Bitplane);
    m_pModeComboBox->addItem(tr("1 Plane"), Mode::k1Bitplane);

    m_pWidthSpinBox->setRange(1, 40);
    m_pWidthSpinBox->setValue(m_width);

    m_pHeightSpinBox->setRange(16, 256);
    m_pHeightSpinBox->setValue(m_height);

    auto pMainGroupBox = new QWidget(this);

    QHBoxLayout *hlayout = new QHBoxLayout();
    SetMargins(hlayout);
    hlayout->addWidget(m_pLineEdit);
    hlayout->addWidget(m_pModeComboBox);
    hlayout->addWidget(new QLabel(tr("Width"), this));
    hlayout->addWidget(m_pWidthSpinBox);
    hlayout->addWidget(new QLabel(tr("Height"), this));
    hlayout->addWidget(m_pHeightSpinBox);
    QWidget* pTopContainer = new QWidget(this);
    pTopContainer->setLayout(hlayout);

    QHBoxLayout *hlayout2 = new QHBoxLayout();
    SetMargins(hlayout2);
    hlayout2->addWidget(m_pLockToVideoCheckBox);
    hlayout2->addWidget(m_pInfoLabel);
    QWidget* pTopContainer2 = new QWidget(this);
    pTopContainer2->setLayout(hlayout2);

    QVBoxLayout *vlayout = new QVBoxLayout;
    SetMargins(vlayout);
    vlayout->addWidget(pTopContainer);
    vlayout->addWidget(pTopContainer2);
    vlayout->addWidget(m_pImageWidget);
    vlayout->setAlignment(Qt::Alignment(Qt::AlignTop));
    pMainGroupBox->setLayout(vlayout);

    setWidget(pMainGroupBox);
    m_pLockToVideoCheckBox->setChecked(true);

    connect(m_pTargetModel,  &TargetModel::connectChangedSignal,          this, &GraphicsInspectorWidget::connectChangedSlot);
    connect(m_pTargetModel,  &TargetModel::startStopChangedSignalDelayed, this, &GraphicsInspectorWidget::startStopChangedSlot);
    connect(m_pTargetModel,  &TargetModel::memoryChangedSignal,           this, &GraphicsInspectorWidget::memoryChangedSlot);
    connect(m_pTargetModel,  &TargetModel::otherMemoryChanged,            this, &GraphicsInspectorWidget::otherMemoryChangedSlot);

    connect(m_pLineEdit,     &QLineEdit::returnPressed,                   this, &GraphicsInspectorWidget::textEditChangedSlot);
    connect(m_pLockToVideoCheckBox,
                             &QCheckBox::stateChanged,                    this, &GraphicsInspectorWidget::lockToVideoChangedSlot);

    connect(m_pModeComboBox, SIGNAL(currentIndexChanged(int)),            SLOT(modeChangedSlot(int)));
    connect(m_pWidthSpinBox, SIGNAL(valueChanged(int)),                   SLOT(widthChangedSlot(int)));
    connect(m_pHeightSpinBox,SIGNAL(valueChanged(int)),                   SLOT(heightChangedSlot(int)));

    connect(m_pImageWidget,  &NonAntiAliasImage::StringChanged,           this, &GraphicsInspectorWidget::StringChangedSlot);

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
    m_mode = static_cast<Mode>(settings.value("mode", QVariant((int)Mode::k4Bitplane)).toInt());

    m_pWidthSpinBox->setValue(m_width);
    m_pHeightSpinBox->setValue(m_height);
    m_pLockToVideoCheckBox->setChecked(settings.value("lockToVideo", QVariant(true)).toBool());
    m_pModeComboBox->setCurrentIndex(m_mode);
    settings.endGroup();
}

void GraphicsInspectorWidget::saveSettings()
{
    QSettings settings;
    settings.beginGroup("GraphicsInspector");

    settings.setValue("geometry", saveGeometry());
    settings.setValue("width", m_width);
    settings.setValue("height", m_height);
    settings.setValue("lockToVideo", m_pLockToVideoCheckBox->isChecked());
    settings.setValue("mode", static_cast<int>(m_mode));
    settings.endGroup();
}

void GraphicsInspectorWidget::keyPressEvent(QKeyEvent* ev)
{
    int offset = 0;

    int32_t bytes = BytesPerMode(GetEffectiveMode());
    int32_t width = GetEffectiveWidth();
    int32_t height = GetEffectiveHeight();

    if (ev->key() == Qt::Key::Key_Up)
        offset = -width * bytes;
    else if (ev->key() == Qt::Key::Key_Down)
        offset = +width * bytes;
    else if (ev->key() == Qt::Key::Key_PageUp)
        offset = height * -width * bytes;
    else if (ev->key() == Qt::Key::Key_PageDown)
        offset = height * width * bytes;
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
        m_pLockToVideoCheckBox->setChecked(false);
        RequestMemory();
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
    // Request new memory for the view
    if (!m_pTargetModel->IsRunning())
    {
        if (m_pLockToVideoCheckBox->isChecked())
            SetAddressFromVideo();

        // Just request what we had already.
        RequestMemory();
    }
}

void GraphicsInspectorWidget::memoryChangedSlot(int /*memorySlot*/, uint64_t commandId)
{
    // Only update for the last request we added
    if (commandId == m_requestIdBitmap)
    {
        const Memory* pMemOrig = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspector);
        if (!pMemOrig)
            return;

        Mode mode = GetEffectiveMode();
        int width = GetEffectiveWidth();
        int height = GetEffectiveHeight();
        int32_t bytesPerChunk = BytesPerMode(mode);
        // Uncompress
        int required = width * bytesPerChunk * height;

        // Ensure we have the right size memory
        if (pMemOrig->GetSize() < required)
            return;

        int bitmapSize = width * 16 * height;
        uint8_t* pDestPixels = m_pImageWidget->AllocBitmap(bitmapSize);
        uint8_t* pDestPixelsStart = pDestPixels;
        const uint8_t* pChunk = pMemOrig->GetData();

        if (mode == k4Bitplane)
        {
            for (int i = 0; i < width * height; ++i)
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
        else if (mode == k2Bitplane)
        {
            for (int i = 0; i < width * height; ++i)
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
        else if (mode == k1Bitplane)
        {
            for (int i = 0; i < width * height; ++i)
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

        // Update image in the widget
        m_pImageWidget->setPixmap(width, height);
        m_requestIdBitmap = 0;
        return;
    }
    else if (commandId == m_requestIdVideoRegs)
    {
        const Memory* pMemOrig = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
        if (!pMemOrig)
            return;

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

        // Request video memory area
        int size = height * width * BytesPerMode(mode);
        m_requestIdBitmap = m_pDispatcher->RequestMemory(MemorySlot::kGraphicsInspector, m_address, size);
        m_requestIdVideoRegs = 0;

        // Now update palette
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

        // Colours are ARGB
        m_pImageWidget->m_colours.clear();

        for (uint i = 0; i < 16; ++i)
        {
            uint32_t addr = HardwareST::VIDEO_PALETTE_0 + i*2;
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
}

void GraphicsInspectorWidget::textEditChangedSlot()
{
    std::string expression = m_pLineEdit->text().toStdString();
    uint32_t addr;
    if (!StringParsers::ParseExpression(expression.c_str(), addr,
                                        m_pTargetModel->GetSymbolTable(),
                                        m_pTargetModel->GetRegs()))
    {
        return;
    }
    m_address = addr;
    m_pLockToVideoCheckBox->setChecked(false);
    UpdateUIElements();
    RequestMemory();
}

void GraphicsInspectorWidget::lockToVideoChangedSlot()
{
    bool m_bLockToVideo = m_pLockToVideoCheckBox->isChecked();
    if (m_bLockToVideo)
    {
        SetAddressFromVideo();
        RequestMemory();
    }
    UpdateUIElements();
}

void GraphicsInspectorWidget::modeChangedSlot(int index)
{
    m_mode = static_cast<Mode>(index);
    RequestMemory();
}

void GraphicsInspectorWidget::otherMemoryChangedSlot(uint32_t address, uint32_t size)
{
    // Do a re-request if our memory is touched
    uint32_t ourSize = GetEffectiveHeight() * GetEffectiveWidth() * BytesPerMode(m_mode);
    if (Overlaps(m_address, ourSize, address, size))
        RequestMemory();
}

void GraphicsInspectorWidget::widthChangedSlot(int value)
{
    m_width = value;
    RequestMemory();
}

void GraphicsInspectorWidget::heightChangedSlot(int value)
{
    m_height = value;
    RequestMemory();
}

void GraphicsInspectorWidget::StringChangedSlot()
{
    m_pInfoLabel->setText(m_pImageWidget->GetString());
}

// Request enough memory based on m_rowCount and m_logicalAddr
void GraphicsInspectorWidget::RequestMemory()
{
    if (!m_pTargetModel->IsConnected())
        return;

    // Video data first. Once that has returned we request the necessary amount of memory
    m_requestIdBitmap = -1;
    m_requestIdVideoRegs = m_pDispatcher->RequestMemory(MemorySlot::kGraphicsInspectorVideoRegs, HardwareST::VIDEO_REGS_BASE, 0x70);
}

bool GraphicsInspectorWidget::SetAddressFromVideo()
{
    // Update to current video regs
    const Memory* pVideoRegs = m_pTargetModel->GetMemory(MemorySlot::kVideo);
    if (pVideoRegs->GetSize() > 0)
    {
        uint32_t hi = pVideoRegs->ReadAddressByte(HardwareST::VIDEO_BASE_HI);
        uint32_t mi = pVideoRegs->ReadAddressByte(HardwareST::VIDEO_BASE_MED);
        uint32_t lo = pVideoRegs->ReadAddressByte(HardwareST::VIDEO_BASE_LO);
        // ST machines don't support low byte seting
        if (IsMachineST(m_pTargetModel->GetMachineType()))
            lo = 0;

        m_address = (hi << 16) | (mi << 8) | lo;
        DisplayAddress();
        return true;
    }
    return false;
}

void GraphicsInspectorWidget::DisplayAddress()
{
    m_pLineEdit->setText(QString::asprintf("$%x", m_address));
}

void GraphicsInspectorWidget::UpdateUIElements()
{
    m_pWidthSpinBox->setValue(m_width);
    m_pHeightSpinBox->setValue(m_height);
    m_pModeComboBox->setCurrentIndex(m_mode);

    m_pWidthSpinBox->setEnabled(!m_pLockToVideoCheckBox->isChecked());
    m_pHeightSpinBox->setEnabled(!m_pLockToVideoCheckBox->isChecked());
    m_pModeComboBox->setEnabled(!m_pLockToVideoCheckBox->isChecked());
}

GraphicsInspectorWidget::Mode GraphicsInspectorWidget::GetEffectiveMode() const
{
    if (!m_pLockToVideoCheckBox->isChecked())
        return m_mode;

    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
    if (!pMem)
        return Mode::k4Bitplane;
    uint8_t modeReg = pMem->ReadAddressByte(HardwareST::VIDEO_RESOLUTION) & 3;
    if (modeReg == 0)
        return Mode::k4Bitplane;
    else if (modeReg == 1)
        return Mode::k2Bitplane;

    return Mode::k1Bitplane;
}

int GraphicsInspectorWidget::GetEffectiveWidth() const
{
    if (!m_pLockToVideoCheckBox->isChecked())
        return m_width;

    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
    if (!pMem)
        return Mode::k4Bitplane;
    uint8_t modeReg = pMem->ReadAddressByte(HardwareST::VIDEO_RESOLUTION) & 3;
    if (modeReg == 0)
        return 20;
    else if (modeReg == 1)
        return 40;
    return 40;
}

int GraphicsInspectorWidget::GetEffectiveHeight() const
{
    if (!m_pLockToVideoCheckBox->isChecked())
        return m_height;

    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspectorVideoRegs);
    if (!pMem)
        return Mode::k4Bitplane;
    uint8_t modeReg = pMem->ReadAddressByte(HardwareST::VIDEO_RESOLUTION) & 3;
    if (modeReg == 0)
        return 200;
    else if (modeReg == 1)
        return 200;
    return 400;
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

