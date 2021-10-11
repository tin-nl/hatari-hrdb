#ifndef HARDWAREWINDOW_H
#define HARDWAREWINDOW_H

#include <QDockWidget>
#include <QAbstractItemModel>
#include <QLabel>
#include "../models/memory.h"

class QTreeView;
class QFormLayout;
class QHBoxLayout;
class QLabel;

class TargetModel;
class Dispatcher;
class Session;

namespace Regs
{
    struct FieldDef;
}

class HardwareTreeItem
{
public:
    enum Type
    {
        kHeader,

        // VIDEO
        kVideoRes,
        kVideoHz,
        kVideoBase,

        // MFP
        kMfpEnabledA,
        kMfpMaskA,
        kMfpPendingA,
        kMfpInServiceA,
        kMfpEnabledB,
        kMfpMaskB,
        kMfpPendingB,
        kMfpInServiceB,

        kMfpTimerAMode,
        kMfpTimerAData,
        kMfpTimerBMode,
        kMfpTimerBData,
        kMfpTimerCMode,
        kMfpTimerCData,
        kMfpTimerDMode,
        kMfpTimerDData,
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

class HardwareField;

class ExpandLabel : public QLabel
{
    Q_OBJECT
public:
    ExpandLabel(QWidget* parent);
signals:
    void clicked();

protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
private:
};


class Expander : public QWidget
{
    Q_OBJECT
public:
    // Two widgets, the clickable item
    QWidget*                m_pTop;
    QWidget*                m_pBottom;
    QHBoxLayout*            m_pTopLayout;
    QFormLayout*            m_pBottomLayout;

    ExpandLabel*            m_pButton;

    // Then the form-layout with child items
    Expander(QWidget* parent, QString text);

public slots:
    void buttonPressedSlot();

private:
    void UpdateState();

    QString m_text;
    bool m_expanded;
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

public slots:
    void connectChangedSlot();
    void startStopChangedSlot();
    void memoryChangedSlot(int memorySlot, uint64_t commandId);
    void settingsChangedSlot();

private:
    void addField(QFormLayout* pLayout, const QString& title, const Regs::FieldDef& def);
    void addBitmask(QFormLayout *pLayout, const QString &title, const Regs::FieldDef** defs);

    //QTreeView*          m_pTableView;
    Session*            m_pSession;
    TargetModel*        m_pTargetModel;
    Dispatcher*         m_pDispatcher;

    Memory              m_videoMem;
    Memory              m_mfpMem;

    QVector<HardwareField*>     m_fields;
};

#endif // HARDWAREWINDOW_H
