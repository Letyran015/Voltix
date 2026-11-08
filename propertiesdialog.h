#pragma once
#include "symbolitem.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QVBoxLayout>

class PropertiesDialog : public QDialog
{
public:
    explicit PropertiesDialog(SymbolItem *item, QWidget *parent = nullptr)
        : QDialog(parent), m_item(item)
    {
        setWindowTitle("Propriétés du composant");
        resize(680, 380);

        auto *lay = new QVBoxLayout(this);
        lay->addWidget(new QLabel(item->title()));

        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("Nom :"));
        m_name = new QLineEdit(item->ref);
        row->addWidget(m_name, 1);
        m_showRef = new QCheckBox("afficher dans le schéma");
        m_showRef->setChecked(item->showRef);
        row->addWidget(m_showRef);
        lay->addLayout(row);

        m_table = new QTableWidget(item->props.size(), 4);
        m_table->setHorizontalHeaderLabels({"Afficher", "Nom", "Valeur", "Description"});
        m_table->verticalHeader()->hide();
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);

        for (int i = 0; i < item->props.size(); ++i) {
            const Prop &pr = item->props[i];

            auto *chk = new QTableWidgetItem;
            chk->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            chk->setCheckState(pr.show ? Qt::Checked : Qt::Unchecked);
            m_table->setItem(i, 0, chk);

            auto *nm = new QTableWidgetItem(pr.name);
            nm->setFlags(Qt::ItemIsEnabled);
            m_table->setItem(i, 1, nm);

            auto *val = new QTableWidgetItem(pr.value);
            val->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsSelectable);
            m_table->setItem(i, 2, val);

            auto *desc = new QTableWidgetItem(pr.unit.isEmpty()
                                                  ? pr.description
                                                  : QString("%1 [%2]").arg(pr.description, pr.unit));
            desc->setFlags(Qt::ItemIsEnabled);
            m_table->setItem(i, 3, desc);
        }
        m_table->resizeColumnsToContents();
        lay->addWidget(m_table, 1);

        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply
                                        | QDialogButtonBox::Cancel);
        lay->addWidget(bb);

        connect(bb, &QDialogButtonBox::accepted, this, [this] { apply(); accept(); });
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(bb, &QDialogButtonBox::clicked, this, [this, bb](QAbstractButton *b) {
            if (bb->buttonRole(b) == QDialogButtonBox::ApplyRole)
                apply();
        });
    }

private:
    void apply()
    {
        const QString n = m_name->text().trimmed();
        if (!n.isEmpty())
            m_item->ref = n;
        m_item->showRef = m_showRef->isChecked();
        for (int i = 0; i < m_item->props.size(); ++i) {
            m_item->props[i].show = m_table->item(i, 0)->checkState() == Qt::Checked;
            m_item->props[i].value = m_table->item(i, 2)->text().trimmed();
        }
        m_item->update();
    }

    SymbolItem *m_item;
    QLineEdit *m_name;
    QCheckBox *m_showRef;
    QTableWidget *m_table;
};
