#pragma once
#include "project.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>

class ProjectDialog : public QDialog
{
public:
    explicit ProjectDialog(const ProjectInfo &pi, QWidget *parent = nullptr)
        : QDialog(parent), m_pi(pi)
    {
        setWindowTitle("Cartouche et format de feuille");
        auto *f = new QFormLayout(this);
        m_paper = new QComboBox;
        m_paper->addItems({"A4", "A3"});
        m_paper->setCurrentText(pi.paper);
        m_company = new QLineEdit(pi.company);
        m_address = new QLineEdit(pi.address);
        m_title = new QLineEdit(pi.title);
        m_author = new QLineEdit(pi.author);
        m_rev = new QLineEdit(pi.revision);
        f->addRow("Format (paysage) :", m_paper);
        f->addRow("Société / établissement :", m_company);
        f->addRow("Adresse :", m_address);
        f->addRow("Titre du projet :", m_title);
        f->addRow("Auteur :", m_author);
        f->addRow("Indice de révision :", m_rev);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        f->addRow(bb);
        connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    ProjectInfo info() const
    {
        ProjectInfo r = m_pi;
        r.paper = m_paper->currentText();
        r.company = m_company->text().trimmed();
        r.address = m_address->text().trimmed();
        r.title = m_title->text().trimmed();
        r.author = m_author->text().trimmed();
        r.revision = m_rev->text().trimmed();
        return r;
    }

private:
    ProjectInfo m_pi;
    QComboBox *m_paper;
    QLineEdit *m_company, *m_address, *m_title, *m_author, *m_rev;
};