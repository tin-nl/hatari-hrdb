#include "showaddressactions.h"

#include "../models/targetmodel.h"

ShowAddressActions::ShowAddressActions(TargetModel* pTargetModel) :
    m_rightClickActiveAddress(0),
    m_pTargetModel(pTargetModel)
{
    for (int i = 0; i < kNumDisasmViews; ++i)
        m_pShowDisasmWindowActions[i] = new QAction(QString::asprintf("Show in Disassembly %d", i + 1), this);

    for (int i = 0; i < kNumMemoryViews; ++i)
        m_pShowMemoryWindowActions[i] = new QAction(QString::asprintf("Show in Memory %d", i + 1), this);

    for (int i = 0; i < kNumDisasmViews; ++i)
        connect(m_pShowDisasmWindowActions[i], &QAction::triggered, this, [=] () { this->disasmViewTrigger(i); } );

    for (int i = 0; i < kNumMemoryViews; ++i)
        connect(m_pShowMemoryWindowActions[i], &QAction::triggered, this, [=] () { this->memoryViewTrigger(i); } );
}

ShowAddressActions::~ShowAddressActions()
{

}

void ShowAddressActions::addToMenu(QMenu* pMenu) const
{
    for (int i = 0; i < kNumDisasmViews; ++i)
        pMenu->addAction(m_pShowDisasmWindowActions[i]);

    for (int i = 0; i < kNumMemoryViews; ++i)
        pMenu->addAction(m_pShowMemoryWindowActions[i]);
}

void ShowAddressActions::setAddress(uint32_t address)
{
    m_rightClickActiveAddress = address;
}

void ShowAddressActions::disasmViewTrigger(int windowIndex)
{
    emit m_pTargetModel->addressRequested(windowIndex, false, m_rightClickActiveAddress);
}

void ShowAddressActions::memoryViewTrigger(int windowIndex)
{
    emit m_pTargetModel->addressRequested(windowIndex, true, m_rightClickActiveAddress);
}

