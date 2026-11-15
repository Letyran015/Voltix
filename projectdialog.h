#pragma once
#include "project.h"
#include "sheet.h"

#include <QBuffer>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>

// Cartouche, format et modèle de cadre de la feuille.
// Le premier menu reprend les modèles de folio habituels (Cadre A4 + Cartouche - Paysage, Modèle 1 - Simple...) ;
// les trois menus suivants permettent n'importe quelle combinaison format / orientation / cadre.
class ProjectDialog : public QDialog
{
public:
    explicit ProjectDialog(const ProjectInfo &pi, QWidget *parent = nullptr)
        : QDialog(parent), m_pi(pi)
    {
        setWindowTitle("Cadre, cartouche et format de feuille");
        auto *h = new QHBoxLayout(this);
        auto *f = new QFormLayout;
        h->addLayout(f, 1);

        m_model = new QComboBox;
        for (const SheetModel &m : sheetModels())
            m_model->addItem(QString::fromUtf8(m.name));
        m_model->addItem("Personnalisé");
        m_paper = new QComboBox;
        m_paper->addItems({"A4", "A3", "A2"});
        m_orient = new QComboBox;
        m_orient->addItems({"Paysage", "Portrait"});
        m_frame = new QComboBox;
        m_frame->addItems({"Normal (sans cadre)", "Cadre", "Cadre + Cartouche", "Cadre + Cartouche avec logo",
                           "Modèle 1 - Simple"});
        m_company = new QLineEdit(pi.company);
        m_address = new QLineEdit(pi.address);
        m_title = new QLineEdit(pi.title);
        m_author = new QLineEdit(pi.author);
        m_rev = new QLineEdit(pi.revision);

        auto *logoRow = new QHBoxLayout;
        m_logoBtn = new QPushButton("Choisir une image…");
        m_logoDel = new QPushButton("Retirer");
        m_logoInfo = new QLabel;
        logoRow->addWidget(m_logoBtn);
        logoRow->addWidget(m_logoDel);
        logoRow->addWidget(m_logoInfo, 1);

        f->addRow("Modèle de feuille :", m_model);
        f->addRow("Format :", m_paper);
        f->addRow("Orientation :", m_orient);
        f->addRow("Cadre :", m_frame);
        f->addRow("Logo du cartouche :", logoRow);
        f->addRow("Société / établissement :", m_company);
        f->addRow("Adresse :", m_address);
        f->addRow("Titre du projet :", m_title);
        f->addRow("Auteur :", m_author);
        f->addRow("Indice de révision :", m_rev);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        f->addRow(bb);
        connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

        m_preview = new QLabel;
        m_preview->setFixedSize(330, 330);
        m_preview->setAlignment(Qt::AlignCenter);
        m_preview->setStyleSheet("QLabel { background:#5a5d63; }");
        h->addWidget(m_preview);

        m_paper->setCurrentText(pi.paper);
        m_orient->setCurrentIndex(pi.portrait ? 1 : 0);
        m_frame->setCurrentIndex(qBound(0, pi.frame, 4));
        syncModel();

        // choisir un modèle règle les trois menus ; modifier un menu retrouve le modèle correspondant
        connect(m_model, &QComboBox::activated, this, [this](int i) {
            const QList<SheetModel> &ms = sheetModels();
            if (i < 0 || i >= ms.size())
                return;
            m_folios = ms[i].folios;
            m_paper->setCurrentText(ms[i].paper);
            m_orient->setCurrentIndex(ms[i].portrait ? 1 : 0);
            m_frame->setCurrentIndex(ms[i].frame);
            refresh();
        });
        for (QComboBox *c : {m_paper, m_orient, m_frame})
            connect(c, &QComboBox::activated, this, [this] { m_folios = 1; syncModel(); refresh(); });
        for (QLineEdit *e : {m_company, m_address, m_title, m_author, m_rev})
            connect(e, &QLineEdit::textChanged, this, [this] { refresh(); });
        connect(m_logoBtn, &QPushButton::clicked, this, [this] {
            const QString p = QFileDialog::getOpenFileName(this, "Logo du cartouche", QString(),
                                                           "Images (*.png *.jpg *.jpeg *.bmp *.svg)");
            if (p.isEmpty())
                return;
            QImage img(p);
            if (img.isNull())
                return;
            if (img.width() > 600 || img.height() > 600)
                img = img.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QByteArray png;
            QBuffer buf(&png);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
            m_pi.logo = png;
            if (m_frame->currentIndex() != FrameLogo) {      // un logo n'apparaît que dans le cartouche avec logo
                m_frame->setCurrentIndex(FrameLogo);
                syncModel();
            }
            refresh();
        });
        connect(m_logoDel, &QPushButton::clicked, this, [this] { m_pi.logo.clear(); refresh(); });
        refresh();
    }

    ProjectInfo info() const
    {
        ProjectInfo r = m_pi;
        r.paper = m_paper->currentText();
        r.portrait = m_orient->currentIndex() == 1;
        r.frame = m_frame->currentIndex();
        r.company = m_company->text().trimmed();
        r.address = m_address->text().trimmed();
        r.title = m_title->text().trimmed();
        r.author = m_author->text().trimmed();
        r.revision = m_rev->text().trimmed();
        return r;
    }
    int foliosWanted() const { return m_folios; }      // modèle « 2 folios » : nombre minimal de folios

private:
    void syncModel()
    {
        const int i = sheetModelOf(info());
        m_model->setCurrentIndex(i < 0 ? m_model->count() - 1 : i);
    }
    void refresh()
    {
        const ProjectInfo pi = info();
        m_logoInfo->setText(pi.logo.isEmpty() ? "aucun" : "image chargée");
        m_logoDel->setEnabled(!pi.logo.isEmpty());
        // aperçu de la feuille
        const QSizeF sz = SheetItem::paperSize(pi);
        const qreal k = 310.0 / qMax(sz.width(), sz.height());
        QPixmap pm(qRound(sz.width() * k), qRound(sz.height() * k));
        pm.fill(Qt::white);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(k, k);
        SheetItem sheet;
        sheet.setShowGrid(false);
        sheet.setInfo(pi, 0, qMax(1, m_folios), "Folio 1");
        sheet.paint(&p, nullptr, nullptr);
        p.end();
        m_preview->setPixmap(pm);
    }

    ProjectInfo m_pi;
    int m_folios = 1;
    QComboBox *m_model, *m_paper, *m_orient, *m_frame;
    QLineEdit *m_company, *m_address, *m_title, *m_author, *m_rev;
    QPushButton *m_logoBtn, *m_logoDel;
    QLabel *m_logoInfo, *m_preview;
};
