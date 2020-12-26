#ifndef GRAPHICSINSPECTOR_H
#define GRAPHICSINSPECTOR_H

#include <QDockWidget>
#include <QObject>

// Forward declarations
class QLabel;
class QLineEdit;
class QAbstractItemModel;
class QSpinBox;

class TargetModel;
class Dispatcher;

// Taken from https://forum.qt.io/topic/94996/qlabel-and-image-antialiasing/5
class NonAntiAliasImage : public QWidget{
    Q_OBJECT
    Q_DISABLE_COPY(NonAntiAliasImage)
public:
    explicit NonAntiAliasImage(QWidget* parent = Q_NULLPTR)
        : QWidget(parent)
    {}
    const QPixmap& pixmap() const
    {
        return m_pixmap;
    }
    void setPixmap(const QPixmap& px)
    {
        m_pixmap = px;
        update();
    }
protected:
    void paintEvent(QPaintEvent*);
private:
    QPixmap m_pixmap;
};


class GraphicsInspectorWidget : public QDockWidget
{
    Q_OBJECT
public:
    GraphicsInspectorWidget(QWidget *parent,
                            TargetModel* pTargetModel, Dispatcher* pDispatcher);
    ~GraphicsInspectorWidget();

    void startStopChangedSlot();
    void memoryChangedSlot(int memorySlot, uint64_t commandId);
    void textEditChangedSlot();

public slots:
    void widthChangedSlot(int width);
    void heightChangedSlot(int height);

private:
    void RequestMemory();

    QLineEdit*      m_pLineEdit;
    QSpinBox*       m_pWidthSpinBox;
    QSpinBox*       m_pHeightSpinBox;
    NonAntiAliasImage*         m_pPictureLabel;

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
    QAbstractItemModel* m_pSymbolTableModel;

    uint32_t        m_address;
    int             m_width;
    int             m_height;
    uint64_t        m_requestId;
};

#endif // GRAPHICSINSPECTOR_H
