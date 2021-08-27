#ifndef SHOWADDRESSACTIONS_H
#define SHOWADDRESSACTIONS_H

#include <QMenu>
#include "../models/memory.h"

class QAction;
class TargetModel;

// Contains the QAction sets to do "Show in Disassembly 1", "Show in Memory 2" etc
class ShowAddressActions : public QObject
{
    Q_OBJECT
public:
    ShowAddressActions(TargetModel* pTargetModel);
    virtual ~ShowAddressActions();
    void addToMenu(QMenu* pMenu) const;

    void setAddress(uint32_t address);

public slots:
    // Callbacks when "show in Memory X" etc is selected
    void disasmViewTrigger(int windowIndex);
    void memoryViewTrigger(int windowIndex);
private:

    uint32_t     m_rightClickActiveAddress;    // What address will be set to the Window chosen

    // Actions to add to the menus
    QAction*     m_pShowDisasmWindowActions[kNumDisasmViews];
    QAction*     m_pShowMemoryWindowActions[kNumMemoryViews];
    TargetModel* m_pTargetModel;
};

#endif // SHOWADDRESSACTIONS_H
