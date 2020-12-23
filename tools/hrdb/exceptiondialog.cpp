#include "exceptiondialog.h"
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "dispatcher.h"

ExceptionDialog::ExceptionDialog(QWidget *parent, TargetModel* pTargetModel, Dispatcher* pDispatcher) :
    QDialog(parent),
    m_pTargetModel(pTargetModel),
    m_pDispatcher(pDispatcher)
{
    QVBoxLayout* pLayout = new QVBoxLayout(this);
    for (int i = 0; i < ExceptionMask::kExceptionCount; ++i)
    {
        m_pCheckboxes[i] = new QCheckBox(ExceptionMask::GetName(i + 2), this);
        pLayout->addWidget(m_pCheckboxes[i]);
    }
    QPushButton* pOkButton = new QPushButton("OK", this);
    pLayout->addWidget(pOkButton);
    //QPushButton* pCancelButton = new QPushButton("Cancel", this);

    connect(pOkButton, &QPushButton::clicked, this, &ExceptionDialog::okClicked);
    this->setLayout(pLayout);
}

ExceptionDialog::~ExceptionDialog()
{

}

void ExceptionDialog::okClicked()
{
    ExceptionMask mask;
    mask.m_mask = 0;

    if (m_pCheckboxes[0]->isChecked()) mask.Set(ExceptionMask::kBus);
    if (m_pCheckboxes[1]->isChecked()) mask.Set(ExceptionMask::kAddress);
    if (m_pCheckboxes[2]->isChecked()) mask.Set(ExceptionMask::kIllegal);
    if (m_pCheckboxes[3]->isChecked()) mask.Set(ExceptionMask::kZeroDiv);
    if (m_pCheckboxes[4]->isChecked()) mask.Set(ExceptionMask::kChk);
    if (m_pCheckboxes[5]->isChecked()) mask.Set(ExceptionMask::kTrapv);
    if (m_pCheckboxes[6]->isChecked()) mask.Set(ExceptionMask::kPrivilege);
    if (m_pCheckboxes[7]->isChecked()) mask.Set(ExceptionMask::kTrace);

    // Send to target
    m_pDispatcher->SendCommandPacket(QString::asprintf("exmask %u", mask.m_mask).toStdString().c_str());
}
