#pragma once
#include "wireitem.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>

class WireDialog : public QDialog
{
public:
    explicit WireDialog(WireItem *w, QWidget *parent = nullptr)
        : QDialog(parent), m_w(w), m_color(w->color())
    {
        setWindowTitle("Propriétés du fil");
        auto *form = new QFormLayout(this);

        m_nature = new QComboBox;
        for (const WireNature &n : wireNatures())
            m_nature->addItem(QString::fromUtf8(n.label), QString::fromLatin1(n.id));
        m_nature->setCurrentIndex(qMax(0, m_nature->findData(w->nature())));

        m_colorBtn = new QPushButton;
        updateButton();

        m_section = new QComboBox;
        m_section->setEditable(true);
        m_section->addItems({"", "0.5", "0.75", "1", "1.5", "2.5", "4", "6", "10", "16", "25", "35", "50"});
        m_section->setCurrentText(w->section());

        m_mark = new QLineEdit(w->mark());
        m_mark->setMaxLength(12);

        form->addRow("Nature :", m_nature);
        form->addRow("Couleur :", m_colorBtn);
        form->addRow("Section (mm²) :", m_section);
        form->addRow("Repère du fil :", m_mark);

        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(bb);

        connect(m_nature, &QComboBox::currentIndexChanged, this, [this] {
            m_color = wireNatureColor(m_nature->currentData().toString());
            updateButton();
        });
        connect(m_colorBtn, &QPushButton::clicked, this, [this] {
            const QColor c = QColorDialog::getColor(m_color, this, "Couleur du fil");
            if (c.isValid()) { m_color = c; updateButton(); }
        });
        connect(bb, &QDialogButtonBox::accepted, this, [this] { apply(); accept(); });
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

private:
    void updateButton()
    {
        m_colorBtn->setText(m_color.name());
        m_colorBtn->setStyleSheet(QString("background:%1; color:%2;")
                                      .arg(m_color.name(), m_color.lightness() < 128 ? "white" : "black"));
    }

    void apply()
    {
        QString s = m_section->currentText().trimmed();
        s.replace(',', '.');
        static const QRegularExpression ok("^[0-9]*\\.?[0-9]*$");
        if (!ok.match(s).hasMatch())
            s.clear();
        m_w->setNature(m_nature->currentData().toString(), false);
        m_w->setColor(m_color);
        m_w->setSection(s);
        m_w->setMark(m_mark->text().trimmed());
    }

    WireItem *m_w;
    QColor m_color;
    QComboBox *m_nature, *m_section;
    QPushButton *m_colorBtn;
    QLineEdit *m_mark;
};