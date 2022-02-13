#ifndef HARDWAREWINDOW_H
#define HARDWAREWINDOW_H

#include <QDockWidget>
#include <QAbstractItemModel>
#include <QTreeView>
#include "../models/memory.h"
#include "../models/session.h"
#include "showaddressactions.h"

class QTreeView;
class QGridLayout;
class QHBoxLayout;
class QLabel;

class TargetModel;
class Dispatcher;
class Session;

namespace Regs
{
    struct FieldDef;
}

class HardwareBase;
class HardwareField;

class HardwareTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit HardwareTreeModel(QObject *parent = nullptr);
    virtual ~HardwareTreeModel() override;
    void dataChanged2(HardwareBase* pField);
    void UpdateSettings(const Session::Settings &settings);
    QModelIndex createIndex2(HardwareBase* pItem) const;

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    HardwareBase *rootItem;
    QFont m_font;
    QFont m_fontBold;
};

class HardwareTreeView : public QTreeView
{
public:
    HardwareTreeView(QWidget* parent, Session* pSession);
protected:
    virtual void contextMenuEvent(QContextMenuEvent *event) override;

    // Menu actions
    QMenu*              m_pShowAddressMenu;
    ShowAddressActions  m_showAddressActions;
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
    void copyToClipboardSlot();
    void connectChangedSlot();
    void startStopChangedSlot();
    void memoryChangedSlot(int memorySlot, uint64_t commandId);
    void ymChangedSlot();
    void settingsChangedSlot();

private:
    void addField(HardwareBase* pLayout, const QString& title, const Regs::FieldDef& def);
    void addMultiField(HardwareBase *pLayout, const QString &title, const Regs::FieldDef** defs);
    void addShared(HardwareBase *pLayout, const QString &title, HardwareField* pField);

    Session*                    m_pSession;
    TargetModel*                m_pTargetModel;
    Dispatcher*                 m_pDispatcher;

    Memory                      m_videoMem;
    Memory                      m_mfpMem;

    HardwareTreeView*           m_pView;
    HardwareTreeModel*          m_pModel;
    QVector<HardwareField*>     m_fields;
};

#endif // HARDWAREWINDOW_H
