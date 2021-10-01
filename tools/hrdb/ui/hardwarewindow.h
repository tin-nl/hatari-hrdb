#ifndef HARDWAREWINDOW_H
#define HARDWAREWINDOW_H

#include <QDockWidget>
#include <QAbstractItemModel>
#include "../models/memory.h"

class QTreeView;
class TargetModel;
class Dispatcher;
class Session;

class HardwareTreeItem
{
public:
    enum Type
    {
        kHeader,

        kVideoRes,
        kVideoHz,
        kVideoBase,

        kMfpTimerAMode,
        kMfpTimerAData,
        kMfpTimerBMode,
        kMfpTimerBData,
    };

    static const uint32_t kMemTypeVideo = 1 << 0;
    static const uint32_t kMemTypeMfp   = 1 << 1;

    HardwareTreeItem(const char* headerName, uint32_t memTypes, Type type);
    ~HardwareTreeItem();

    void appendChild(HardwareTreeItem *child);

    HardwareTreeItem *child(int row);
    int childCount() const;

    // This is "my own index inside my parent"
    int row() const;
    HardwareTreeItem *parentItem();

    bool m_isHeader;
    uint32_t m_memTypes;
    Type m_type;
    const char* m_title;

private:
    QVector<HardwareTreeItem*> m_childItems;
    HardwareTreeItem *m_parentItem;
};

class HardwareTableModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Column
    {
        kColName,
        kColData,
        kColCount
    };

    HardwareTableModel(QObject * parent, TargetModel* pTargetModel, Dispatcher* pDispatcher);
    virtual ~HardwareTableModel();

    // "When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data()."
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    virtual QModelIndex index(int, int, const QModelIndex&) const;
    virtual QModelIndex parent(const QModelIndex&) const;

public slots:
    void startStopChangedSlot();
    void memoryChangedSlot(int memorySlot, uint64_t commandId);

private:
    void emitDataChange(HardwareTreeItem* root, uint32_t memTypes);

    QString getData(HardwareTreeItem::Type type) const;

    TargetModel*    m_pTargetModel;
    Dispatcher*     m_pDispatcher;

    // Memory and requests
    uint64_t            m_videoRequest;
    uint64_t            m_mfpRequest;

    Memory              m_videoMem;
    Memory              m_mfpMem;

    HardwareTreeItem*   m_pRootItem;
};

class HardwareWindow : public QDockWidget
{
    Q_OBJECT
public:
    HardwareWindow(QWidget *parent, Session* pSession);
    virtual ~HardwareWindow();

    // Grab focus and point to the main widget
    void keyFocus();

    void loadSettings();
    void saveSettings();

private slots:
    void connectChangedSlot();
    void settingsChangedSlot();

private:

    QTreeView*          m_pTableView;
    Session*            m_pSession;
    TargetModel*        m_pTargetModel;
    Dispatcher*         m_pDispatcher;
};

#endif // HARDWAREWINDOW_H
