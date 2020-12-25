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
    QLabel*         m_pPictureLabel;

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
    QAbstractItemModel* m_pSymbolTableModel;

    uint32_t        m_address;
    int             m_width;
    int             m_height;
    uint64_t        m_requestId;
};

#endif // GRAPHICSINSPECTOR_H
