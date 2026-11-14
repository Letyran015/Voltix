// Voltix - fenêtre « Modifier un symbole » des symboles d'électrotechnique (bibliothèque elib).
// Organisation proche des logiciels de schémas d'armoire : Champs 1 à 4 (Nom, Valeur, Champ 3, Champ 4
// avec leur visibilité), Broche (numéros), Référence croisée (maître / esclave) et aperçu du symbole.
#pragma once
#include "elib.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

class ElectroPropertiesDialog : public QDialog
{
public:
    explicit ElectroPropertiesDialog(LibItem *item, QWidget *parent = nullptr)
        : QDialog(parent), m_item(item)
    {
        const bool slave = item->isSlave();
        const QString shownName = slave ? value("Bobine") : item->ref;
        setWindowTitle(QString("Modifier un symbole : %1 - %2").arg(shownName, item->title()));
        resize(860, 430);

        auto *root = new QVBoxLayout(this);
        auto *body = new QHBoxLayout;
        root->addLayout(body, 1);

        auto *pagesList = new QListWidget;
        pagesList->setFixedWidth(190);
        auto *stack = new QStackedWidget;
        body->addWidget(pagesList);
        body->addWidget(stack, 1);

        // ------------------------------------------------------------ Champs 1 à 4
        auto *p1 = new QWidget;
        auto *l1 = new QVBoxLayout(p1);
        l1->addWidget(heading("Champs 1 à 4"));
        auto *g = new QGridLayout;
        g->addWidget(new QLabel("<b>Vis.</b>"), 0, 2);
        m_name = new QLineEdit(shownName);
        m_nameVis = new QCheckBox;
        m_nameVis->setChecked(slave ? shown("Bobine") : item->showRef);
        g->addWidget(new QLabel("<b>Nom</b>"), 1, 0);
        g->addWidget(m_name, 1, 1);
        g->addWidget(m_nameVis, 1, 2);
        int row = 2;
        if (slave)
            g->addWidget(note("( Symbole esclave : nom de la bobine ou de l'appareil maître )"), row++, 1);

        m_val = new QLineEdit(value("Valeur"));
        m_valVis = new QCheckBox;
        m_valVis->setChecked(shown("Valeur"));
        g->addWidget(new QLabel("<b>Valeur</b>"), row, 0);
        g->addWidget(m_val, row, 1);
        g->addWidget(m_valVis, row, 2);
        ++row;
        if (slave) {
            m_val->setPlaceholderText(item->xref.isEmpty() ? "référence croisée (calculée)" : item->xref);
            g->addWidget(note(item->xref.isEmpty()
                                  ? "( Symbole esclave : la référence croisée s'affichera dès que le maître existe )"
                                  : QString("( Symbole esclave : référence croisée %1 )").arg(item->xref)), row++, 1);
        }
        m_c3 = new QLineEdit(value("Champ 3"));
        m_c3Vis = new QCheckBox;
        m_c3Vis->setChecked(shown("Champ 3"));
        g->addWidget(new QLabel("Champ 3"), row, 0);
        g->addWidget(m_c3, row, 1);
        g->addWidget(m_c3Vis, row, 2);
        ++row;
        m_c4 = new QLineEdit(value("Champ 4"));
        m_c4Vis = new QCheckBox;
        m_c4Vis->setChecked(shown("Champ 4"));
        g->addWidget(new QLabel("Champ 4"), row, 0);
        g->addWidget(m_c4, row, 1);
        g->addWidget(m_c4Vis, row, 2);
        g->setColumnStretch(1, 1);
        l1->addLayout(g);

        l1->addSpacing(8);
        l1->addWidget(link("Information"));
        auto *info = new QFormLayout;
        info->addRow("Dossier :", new QLabel(item->folder()));
        info->addRow("Symbole :", new QLabel(item->title()));
        info->addRow("Réf. croisées :", new QLabel(item->isMaster() ? "Maître" : slave ? "Esclave" : "Néant"));
        l1->addLayout(info);
        l1->addStretch(1);
        stack->addWidget(p1);
        pagesList->addItem("Champs 1 à 4");

        // ------------------------------------------------------------ Broche
        auto *p2 = new QWidget;
        auto *l2 = new QVBoxLayout(p2);
        l2->addWidget(heading("Broche"));
        m_numbers = new QCheckBox("Afficher les numéros");
        m_numbers->setChecked(value("Numéros") != "0");
        l2->addWidget(m_numbers);
        if (has("Bornes")) {
            l2->addSpacing(8);
            l2->addWidget(link("Numéro"));
            auto *f = new QFormLayout;
            m_bornes = new QLineEdit(value("Bornes"));
            f->addRow("Numéros des bornes :", m_bornes);
            l2->addLayout(f);
            l2->addWidget(note("Séparer les numéros par un tiret, dans l'ordre des bornes : 13-14, A1-A2, 11-12-14..."));
        }
        l2->addStretch(1);
        stack->addWidget(p2);
        pagesList->addItem("Broche");

        // ------------------------------------------------------------ Référence croisée
        if (item->isMaster() || slave) {
            auto *p3 = new QWidget;
            auto *l3 = new QVBoxLayout(p3);
            l3->addWidget(heading(item->isMaster() ? "Réf. Croisée (Maître)" : "Réf. Croisée (Esclave)"));
            if (item->isMaster()) {
                l3->addWidget(new QLabel("Contacts et pôles liés à ce symbole ( folio - colonne ) ; "
                                         "« P » désigne des pôles de puissance :"));
                auto *cols = new QHBoxLayout;
                auto *no = new QListWidget;
                no->addItems(item->xrefNo);
                auto *nc = new QListWidget;
                nc->addItems(item->xrefNc);
                auto *a = new QVBoxLayout;
                a->addWidget(new QLabel("<b>NO</b>"));
                a->addWidget(no);
                auto *b = new QVBoxLayout;
                b->addWidget(new QLabel("<b>NC</b>"));
                b->addWidget(nc);
                cols->addLayout(a);
                cols->addLayout(b);
                l3->addLayout(cols, 1);
                if (item->xrefNo.isEmpty() && item->xrefNc.isEmpty())
                    l3->addWidget(note("Aucun contact ne porte encore le nom de ce symbole."));
            } else {
                l3->addWidget(link("Liaison"));
                l3->addWidget(new QLabel(item->xref.isEmpty()
                                             ? QString("Aucun maître « %1 » trouvé dans le projet.").arg(shownName)
                                             : QString("Ce symbole a pour maître : %1  %2").arg(shownName, item->xref)));
                l3->addWidget(note("Pour changer de maître, modifier le champ Nom (page Champs 1 à 4)."));
                l3->addStretch(1);
            }
            stack->addWidget(p3);
            pagesList->addItem(item->isMaster() ? "Réf. Croisée (Maître)" : "Réf. Croisée (Esclave)");
        }

        // ------------------------------------------------------------ aperçu
        auto *right = new QVBoxLayout;
        right->addWidget(new QLabel("<b>Symbole</b>"));
        auto *pv = new QLabel;
        pv->setFixedSize(230, 230);
        pv->setFrameShape(QFrame::Box);
        pv->setPixmap(preview(item->kind(), 228));
        right->addWidget(pv);
        right->addStretch(1);
        body->addLayout(right);

        connect(pagesList, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
        pagesList->setCurrentRow(0);

        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        bb->button(QDialogButtonBox::Cancel)->setText("Annuler");
        root->addWidget(bb);
        connect(bb, &QDialogButtonBox::accepted, this, [this] { apply(); accept(); });
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

private:
    static QLabel *heading(const QString &t)
    {
        auto *l = new QLabel(QString("<span style='font-size:14pt;font-weight:600'>%1</span>").arg(t));
        return l;
    }
    static QLabel *link(const QString &t)
    {
        return new QLabel(QString("<u><b>%1</b></u>").arg(t));
    }
    static QLabel *note(const QString &t)
    {
        auto *l = new QLabel(t);
        l->setStyleSheet("color:#c0392b;");
        l->setWordWrap(true);
        return l;
    }

    static QPixmap preview(const QString &kind, int size)
    {
        QPixmap pm(size, size);
        pm.fill(Qt::white);
        LibItem *s = dynamic_cast<LibItem *>(makeLibSymbol(kind));
        if (!s)
            return pm;
        s->showRef = false;
        for (Prop &pr : s->props)
            pr.show = false;
        const QRectF b = s->body();
        const qreal k = qMin(3.0, (size - 24) / qMax(b.width(), b.height()));
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(size / 2.0, size / 2.0);
        p.scale(k, k);
        p.translate(-b.center());
        s->paint(&p, nullptr, nullptr);
        p.end();
        delete s;
        return pm;
    }

    bool has(const QString &name) const
    {
        for (const Prop &pr : m_item->props)
            if (pr.name == name)
                return true;
        return false;
    }
    QString value(const QString &name) const
    {
        for (const Prop &pr : m_item->props)
            if (pr.name == name)
                return pr.value;
        return QString();
    }
    bool shown(const QString &name) const
    {
        for (const Prop &pr : m_item->props)
            if (pr.name == name)
                return pr.show;
        return false;
    }
    void set(const QString &name, const QString &v, bool show)
    {
        for (Prop &pr : m_item->props)
            if (pr.name == name) {
                pr.value = v.trimmed();
                pr.show = show;
            }
    }

    void apply()
    {
        const QString n = m_name->text().trimmed();
        if (m_item->isSlave()) {
            set("Bobine", n.isEmpty() ? value("Bobine") : n, m_nameVis->isChecked());
        } else {
            if (!n.isEmpty())
                m_item->ref = n;
            m_item->showRef = m_nameVis->isChecked();
        }
        set("Valeur", m_val->text(), m_valVis->isChecked());
        set("Champ 3", m_c3->text(), m_c3Vis->isChecked());
        set("Champ 4", m_c4->text(), m_c4Vis->isChecked());
        set("Numéros", m_numbers->isChecked() ? "1" : "0", false);
        if (m_bornes)
            set("Bornes", m_bornes->text(), false);
        m_item->update();
    }

    LibItem *m_item;
    QLineEdit *m_name, *m_val, *m_c3, *m_c4, *m_bornes = nullptr;
    QCheckBox *m_nameVis, *m_valVis, *m_c3Vis, *m_c4Vis, *m_numbers;
};
