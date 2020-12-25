#ifndef GRAPHICSINSPECTOR_H
#define GRAPHICSINSPECTOR_H

#include <QDockWidget>

// Forward declarations
class QLabel;
class QLineEdit;
class QAbstractItemModel;
class TargetModel;
class Dispatcher;

class GraphicsInspectorWidget : public QDockWidget
{
public:
    GraphicsInspectorWidget(QWidget *parent,
                            TargetModel* pTargetModel, Dispatcher* pDispatcher);
private slots:
    void startStopChangedSlot();
    void memoryChangedSlot(int memorySlot, uint64_t commandId);
    void textEditChangedSlot();

private:
    void RequestMemory();

    QLineEdit*      m_pLineEdit;
    QLabel*         m_pPictureLabel;

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;
    QAbstractItemModel* m_pSymbolTableModel;

    uint32_t        m_address;
    uint64_t        m_requestId;

};

#endif // GRAPHICSINSPECTOR_H
