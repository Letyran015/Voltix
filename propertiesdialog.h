#pragma once
#include "symbolitem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QMap>
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

        const auto presets = presetList(item->kind());
        if (!presets.isEmpty()) {
            auto *pr = new QHBoxLayout;
            pr->addWidget(new QLabel("Modèle prédéfini :"));
            m_preset = new QComboBox;
            m_preset->addItem("(personnalisé)");
            for (const auto &e : presets) m_preset->addItem(e.first);
            pr->addWidget(m_preset, 1);
            lay->addLayout(pr);
            connect(m_preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this, presets](int idx) {
                        if (idx <= 0) return;
                        const QMap<QString, QString> &vals = presets[idx - 1].second;
                        for (int i = 0; i < m_item->props.size(); ++i) {
                            const QString &nm = m_item->props[i].name;
                            if (vals.contains(nm))
                                m_table->item(i, 2)->setText(vals.value(nm));
                        }
                    });
        }

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
    using PresetEntry = QPair<QString, QMap<QString, QString>>;
    static QList<PresetEntry> presetList(const QString &kind)
    {
        using M = QMap<QString, QString>;
        QList<PresetEntry> l;
        if (kind == "npn") {
            l << PresetEntry{"2N2222A (calé sur nos mesures TP)",
                             M{{"Bf", "115"}, {"Is", "1e-14"}, {"Nf", "1"}, {"Vaf", "15"}, {"Ikf", "0"},
                               {"Rb", "0"}, {"Rc", "9"}, {"Re", "0"}}};
            l << PresetEntry{"2N2222A (paramètres QucsStudio)",
                             M{{"Bf", "929.846"}, {"Is", "1e-14"}, {"Nf", "1.10496"}, {"Vaf", "16.5003"},
                               {"Ikf", "0.019539"}, {"Rb", "0.1"}, {"Rc", "0.426673"}, {"Re", "0.0001"}}};
            l << PresetEntry{"Idéal (Bf=100, sans résistances)",
                             M{{"Bf", "100"}, {"Is", "1e-14"}, {"Nf", "1"}, {"Vaf", "0"}, {"Ikf", "0"},
                               {"Rb", "0"}, {"Rc", "0"}, {"Re", "0"}}};
        } else if (kind == "diode") {
            l << PresetEntry{"1N4007",
                             M{{"Is", "76.9p"}, {"N", "1.45"}, {"Rs", "42m"}, {"Cjo", "26.5p"},
                               {"Tt", "4.32u"}, {"Bv", "1000"}}};
            l << PresetEntry{"1N4148",
                             M{{"Is", "2.52n"}, {"N", "1.752"}, {"Rs", "0.568"}, {"Cjo", "4p"},
                               {"Tt", "20n"}, {"Bv", "100"}}};
            l << PresetEntry{"Idéale (Is=1e-15, N=1)",
                             M{{"Is", "1e-15"}, {"N", "1"}, {"Rs", "0"}, {"Cjo", "0"}, {"Tt", "0"},
                               {"Bv", "0"}}};
        }
        if (kind == "zener") {
            l << PresetEntry{"1N746 (3,3 V, QucsStudio)",
                             M{{"Is", "10p"}, {"N", "1.27"}, {"Rs", "13.65"}, {"Bv", "3.027"}}};
            for (const char *vz : {"3.3", "4.7", "5.1", "6.8", "9.1", "12", "15"})
                l << PresetEntry{QString("Zener %1 V").arg(vz), M{{"Bv", vz}}};
        } else if (kind == "led") {
            l << PresetEntry{"LED de notre SAE (mesurée)",
                             M{{"Is", "1.05452e-19"}, {"N", "1.83272"}, {"Rs", "9.99751"}}};
            l << PresetEntry{"LED infrarouge (~1,2 V)", M{{"Is", "1e-13"}, {"N", "2"}}};
            l << PresetEntry{"LED rouge (~1,9 V)", M{{"Is", "3e-19"}, {"N", "2"}}};
            l << PresetEntry{"LED jaune (~2,1 V)", M{{"Is", "1e-20"}, {"N", "2"}}};
            l << PresetEntry{"LED verte (~2,25 V)", M{{"Is", "1e-21"}, {"N", "2"}}};
            l << PresetEntry{"LED bleue (~3 V)", M{{"Is", "1e-27"}, {"N", "2"}}};
        } else if (kind == "schottky") {
            l << PresetEntry{"BAR46 (paramètres QucsStudio partiels)",
                             M{{"N", "2.3964"}, {"Rs", "0.0077928"}}};
        } else if (kind == "njfet") {
            l << PresetEntry{"BF245A",
                             M{{"Vto", "-1.7372"}, {"Beta", "0.00116621"}, {"Lambda", "0.0177211"},
                               {"Rd", "9.01678"}, {"Rs", "9.01678"}}};
        } else if (kind == "bridge") {
            const struct { const char *n; const char *bv; } br[] = {
                {"DF005M / KBP005G (50 V)", "50"}, {"DF01M / KBP01G (100 V)", "100"},
                {"DF02M / KBP02G (200 V)", "200"}, {"DF04M / KBP04G (400 V)", "400"},
                {"DF06M / KBP06G (600 V)", "600"}, {"DF08M / KBP08G (800 V)", "800"},
                {"DF10M / KBP10G (1000 V)", "1000"}};
            for (const auto &b : br)
                l << PresetEntry{b.n, M{{"Is", "76.9p"}, {"N", "1.45"}, {"Rs", "42m"}, {"Bv", b.bv}}};
        } else if (kind == "nmos") {
            l << PresetEntry{"Seuil logique (Vto = 1 V)", M{{"Vto", "1"}, {"Kp", "0.5"}}};
            l << PresetEntry{"Standard (Vto = 2 V)", M{{"Vto", "2"}, {"Kp", "0.5"}}};
            l << PresetEntry{"Puissance (Vto = 3 V, Kp = 5)", M{{"Vto", "3"}, {"Kp", "5"}}};
        }
        return l;
    }

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
    QComboBox *m_preset = nullptr;
};
