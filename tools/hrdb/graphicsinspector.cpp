#include "graphicsinspector.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QCompleter>
#include <QSpinBox>

#include "dispatcher.h"
#include "targetmodel.h"
#include "symboltablemodel.h"
#include "stringparsers.h"

GraphicsInspectorWidget::GraphicsInspectorWidget(QWidget *parent,
                                                 TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDockWidget(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher),
    m_address(0U),
    m_width(20),
    m_height(200),
    m_requestId(0U)
{
    QString name("Graphics Inspector");
    this->setObjectName(name);
    this->setWindowTitle(name);

    m_pPictureLabel = new QLabel(this);
    m_pPictureLabel->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));

    m_pLineEdit = new QLineEdit(this);
    m_pSymbolTableModel = new SymbolTableModel(this, m_pTargetModel->GetSymbolTable());
    QCompleter* pCompl = new QCompleter(m_pSymbolTableModel, this);
    pCompl->setCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
    m_pLineEdit->setCompleter(pCompl);

    m_pWidthSpinBox = new QSpinBox(this);
    m_pWidthSpinBox->setRange(1, 32);
    m_pWidthSpinBox->setValue(m_width);

    m_pHeightSpinBox = new QSpinBox(this);
    m_pHeightSpinBox->setRange(16, 256);
    m_pHeightSpinBox->setValue(m_height);
    auto pMainGroupBox = new QGroupBox(this);

    QVBoxLayout *vlayout = new QVBoxLayout;
    vlayout->addWidget(m_pLineEdit);
    vlayout->addWidget(m_pWidthSpinBox);
    vlayout->addWidget(m_pHeightSpinBox);
    vlayout->addWidget(m_pPictureLabel);
    vlayout->setAlignment(Qt::Alignment(Qt::AlignTop));
    pMainGroupBox->setFlat(true);
    pMainGroupBox->setLayout(vlayout);

    setWidget(pMainGroupBox);

    connect(m_pTargetModel,  &TargetModel::startStopChangedSignalDelayed, this, &GraphicsInspectorWidget::startStopChangedSlot);
    connect(m_pTargetModel,  &TargetModel::memoryChangedSignal,           this, &GraphicsInspectorWidget::memoryChangedSlot);
    connect(m_pLineEdit,     &QLineEdit::returnPressed,                   this, &GraphicsInspectorWidget::textEditChangedSlot);
    connect(m_pWidthSpinBox, SIGNAL(valueChanged(int)),                   SLOT(widthChangedSlot(int)));
    connect(m_pHeightSpinBox,SIGNAL(valueChanged(int)),                   SLOT(heightChangedSlot(int)));
}

GraphicsInspectorWidget::~GraphicsInspectorWidget()
{

}

void GraphicsInspectorWidget::startStopChangedSlot()
{
    // Request new memory for the view
    if (!m_pTargetModel->IsRunning())
    {
        // Just request what we had already.
        RequestMemory();
    }
}

void GraphicsInspectorWidget::memoryChangedSlot(int /*memorySlot*/, uint64_t commandId)
{
    // Only update for the last request we added
    if (commandId != m_requestId)
        return;

    const Memory* pMemOrig = m_pTargetModel->GetMemory(MemorySlot::kGraphicsInspector);
    if (!pMemOrig)
        return;

    // Need to redraw here
    uint8_t* pBitmap = new uint8_t[m_width*16*m_height];

    // Uncompress
    // NO CHECK ensure we have the right size memory
    if (pMemOrig->GetSize() < m_width * 8 * m_height)
        return;

    const uint8_t* pChunk = pMemOrig->GetData();
    uint8_t* pDestPixels = pBitmap;
    for (int i = 0; i < m_width * m_height; ++i)
    {
        uint16_t pSrc[4];
        pSrc[0] = (pChunk[0] << 8) | pChunk[1];
        pSrc[1] = (pChunk[2] << 8) | pChunk[3];
        pSrc[2] = (pChunk[4] << 8) | pChunk[5];
        pSrc[3] = (pChunk[6] << 8) | pChunk[7];
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

    QImage img(pBitmap, m_width * 16, m_height, QImage::Format_Indexed8);
    QVector<QRgb> colours;
    // Colours are ARGB
    for (uint32_t i = 0; i < 16; ++i)
        colours.append(i * 0x203040 | 0xff000000);
    img.setColorTable(colours);
    m_pPictureLabel->setPixmap(QPixmap::fromImage(img));

    delete [] pBitmap;
    m_requestId = 0;
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

// Request enough memory based on m_rowCount and m_logicalAddr
void GraphicsInspectorWidget::RequestMemory()
{
    uint32_t size = m_height * m_width * 8;
    m_requestId = m_pDispatcher->RequestMemory(MemorySlot::kGraphicsInspector, m_address, size);
}
