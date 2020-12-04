#include "disasmwidget.h"
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QStringListModel>

#include "targetmodel.h"

DisasmWidget::DisasmWidget(QWidget *parent, TargetModel* pTargetModel) :
    QDockWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout;
    auto pGroupBox = new QGroupBox(this);

    m_pLineEdit = new QLineEdit(this);
    m_pTableView = new QTableView(this);

    DisasmTableModel* pModel = new DisasmTableModel(this, pTargetModel);
    m_pTableView->setModel(pModel);
    m_pTableView->horizontalHeader()->hide();
    m_pTableView->verticalHeader()->hide();
    layout->addWidget(m_pLineEdit);
    layout->addWidget(m_pTableView);
    pGroupBox->setLayout(layout);
    setWidget(pGroupBox);

    // TODO Connect slots to detect when the disassembly data has changed

}

DisasmTableModel::DisasmTableModel(QObject *parent, TargetModel *pTargetModel) :
    QAbstractTableModel(parent),
    m_pTargetModel(pTargetModel)
{
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal, this, &DisasmTableModel::memoryChangedSlot);
}

int DisasmTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_disasm.lines.size();
}

int DisasmTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return 3;
}

QVariant DisasmTableModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (index.column() == 0)
        {
            QString addr;
            addr.setNum(m_disasm.lines[index.row()].address, 16);
            return addr;
        }
        else if (index.column() == 1)
        {
            if (m_pTargetModel->GetPC() == m_disasm.lines[index.row()].address)
            {
                return QString(">");
            }
            return QVariant(); // invalid item
        }
        else if (index.column() == 2)
        {
            QString str;
            QTextStream ref(&str);
            Disassembler::print(m_disasm.lines[index.row()].inst,
                    m_disasm.lines[index.row()].address, ref);
            return str;
        }
        return QString("addr");
    }
    return QVariant(); // invalid item
}

void DisasmTableModel::memoryChangedSlot()
{
    m_disasm.lines.clear();
    const Memory* pMem = m_pTargetModel->GetMemory();
    if (!pMem)
    {

        return;
    }

    buffer_reader disasmBuf(pMem->GetData(), pMem->GetSize());
    Disassembler::decode_buf(disasmBuf, m_disasm, pMem->GetAddress());
    emit beginResetModel();
    emit endResetModel();
    //emit modelReset(QPrivateSignal());
}
