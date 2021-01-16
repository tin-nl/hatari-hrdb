#ifndef MEMORYVIEWWIDGET_H
#define MEMORYVIEWWIDGET_H

#include <QDockWidget>
#include <QTableView>
#include "memory.h"

class TargetModel;
class Dispatcher;
class QComboBox;
class QCheckBox;

class MemoryWidget : public QWidget
{
    Q_OBJECT
public:
    enum Column
    {
        kColAddress,
        kColData,
        kColAscii,
        kColCount
    };

    enum Mode
    {
        kModeByte,
        kModeWord,
        kModeLong
    };

    MemoryWidget(QWidget* parent, TargetModel* pTargetModel, Dispatcher* pDispatcher, int windowIndex);

    uint32_t GetRowCount() const { return m_rowCount; }
    Mode GetMode() const { return m_mode; }

    // returns false if expression is invalid
    bool SetAddress(std::string expression);
    void SetRowCount(uint32_t rowCount);
    void SetLock(bool locked);
    void SetMode(Mode mode);

public slots:
    void memoryChangedSlot(int memorySlot, uint64_t commandId);
    void startStopChangedSlot();
    void connectChangedSlot();

protected:
    virtual void paintEvent(QPaintEvent*);
    virtual void keyPressEvent(QKeyEvent*);
private:
    void MoveUp();
    void MoveDown();
    void MoveLeft();
    void MoveRight();
    void PageUp();
    void PageDown();

    void SetAddress(uint32_t address);
    void RequestMemory();
    void RecalcText();
    void resizeEvent(QResizeEvent *event);
    void RecalcRowCount();

    void RecalcSizes();
    int GetAddrX() const;
    int GetHexCharX(int x) const;
    int GetAsciiCharX() const;

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;

    // These are taken at the same time. Is there a race condition...?
    struct Row
    {
        QString m_hexText;
        QString m_asciiText;
    };

    std::vector<Row> m_rows;
    std::vector<uint32_t> m_xpos;

    std::string m_addressExpression;
    bool    m_isLocked;
    uint32_t m_address;

    uint32_t m_bytesPerRow;
    Mode     m_mode;

    uint32_t m_rowCount;
    uint64_t m_requestId;
    int      m_windowIndex;        // e.g. "memory 0", "memory 1"
    MemorySlot  m_memSlot;

    // Cursor
    int     m_cursorRow;
    int     m_cursorCol;


    // rendering info
    int     m_charWidth;
    int     m_lineHeight;           // font height in pixels
    QFont   monoFont;
};

#if 0
class MemoryTableView : public QTableView
{
    Q_OBJECT
public:
    MemoryTableView(QWidget* parent, MemoryViewTableModel* pModel, TargetModel* pTargetModel);

public slots:

protected:
    QModelIndex moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers);
private:
    // override -- this doesn't trigger at the start?
    virtual void resizeEvent(QResizeEvent*);
private slots:
    void RecalcRowCount();

private:
    MemoryViewTableModel*     m_pTableModel;

    // Remembers which row we right-clicked on
    int                   m_rightClickRow;
};
#endif

class MemoryViewWidget : public QDockWidget
{
    Q_OBJECT
public:
    MemoryViewWidget(QWidget *parent, TargetModel* pTargetModel, Dispatcher* m_pDispatcher, int windowIndex);

public slots:
    void requestAddress(int windowIndex, bool isMemory, uint32_t address);

    void textEditChangedSlot();
    void lockChangedSlot();
    void modeComboBoxChanged(int index);

private:
    QLineEdit*           m_pLineEdit;
    QComboBox*           m_pComboBox;
    QCheckBox*           m_pLockCheckBox;
    MemoryWidget*        pModel;

    TargetModel*        m_pTargetModel;
    Dispatcher*         m_pDispatcher;
    QAbstractItemModel* m_pSymbolTableModel;
    int                 m_windowIndex;
};

#endif // MEMORYVIEWWIDGET_H
