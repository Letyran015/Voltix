#pragma once
#include "catalog.h"
#include "symbolitem.h"

#include <QCheckBox>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>

class LibraryPanel : public QWidget
{
public:
    std::function<void(const QString &)> onPick;     // appelé quand un composant est choisi

    explicit LibraryPanel(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(6, 6, 6, 6);

        m_title = new QLabel;
        QFont f = m_title->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 1);
        m_title->setFont(f);
        lay->addWidget(m_title);

        m_search = new QLineEdit;
        m_search->setPlaceholderText("Rechercher un composant…");
        m_search->setClearButtonEnabled(true);
        lay->addWidget(m_search);

        m_all = new QCheckBox("Afficher tous les modules");
        lay->addWidget(m_all);

        m_tree = new QTreeWidget;
        m_tree->setHeaderHidden(true);
        m_tree->setIconSize(QSize(44, 44));
        m_tree->setIndentation(10);
        lay->addWidget(m_tree, 1);

        connect(m_search, &QLineEdit::textChanged, this, [this] { rebuild(); });
        connect(m_all, &QCheckBox::toggled, this, [this] { rebuild(); });
        connect(m_tree, &QTreeWidget::currentItemChanged, this,
                [this](QTreeWidgetItem *cur, QTreeWidgetItem *) {
                    if (!cur)
                        return;
                    const QString id = cur->data(0, Qt::UserRole).toString();
                    if (id.isEmpty())
                        return;                       // clic sur un titre de groupe
                    m_current = id;
                    if (onPick)
                        onPick(id);
                });
    }

    void setModule(int m)
    {
        m_module = m;
        m_title->setText(moduleTitle(m));
        rebuild();
    }

private:
    // petite icône : le symbole lui-même, dessiné sur fond blanc
    static QIcon iconFor(const QString &kind)
    {
        static QHash<QString, QIcon> cache;
        if (cache.contains(kind))
            return cache.value(kind);

        QPixmap pm(88, 88);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(0, 0, 44, 44), 6, 6);

        if (SymbolItem *s = makeSymbol(kind)) {
            s->showRef = false;
            for (Prop &pr : s->props)
                pr.show = false;
            const QRectF r = s->shape().boundingRect();
            qreal sc = qMin(34.0 / r.width(), 34.0 / r.height());
            sc = qMin(sc, 0.8);
            p.translate(22, 22);
            p.scale(sc, sc);
            p.translate(-r.center());
            s->paint(&p, nullptr, nullptr);
            delete s;
        }
        p.end();

        const QIcon ic(pm);
        cache.insert(kind, ic);
        return ic;
    }

    void rebuild()
    {
        const QString needle = m_search->text().trimmed();
        const bool all = m_all->isChecked();

        QSignalBlocker block(m_tree);
        m_tree->clear();

        QHash<QString, QTreeWidgetItem *> groups;
        QTreeWidgetItem *found = nullptr, *first = nullptr;

        for (const CatalogEntry &e : catalog()) {
            if (!all && !(e.modules & m_module))
                continue;
            if (!needle.isEmpty() && !e.name.contains(needle, Qt::CaseInsensitive))
                continue;

            QTreeWidgetItem *g = groups.value(e.group);
            if (!g) {
                g = new QTreeWidgetItem(m_tree, QStringList(e.group));
                g->setFlags(Qt::ItemIsEnabled);
                QFont f = g->font(0);
                f.setBold(true);
                g->setFont(0, f);
                groups.insert(e.group, g);
            }
            auto *it = new QTreeWidgetItem(g, QStringList(e.name));
            it->setIcon(0, iconFor(e.id));
            it->setData(0, Qt::UserRole, e.id);
            if (!first) first = it;
            if (e.id == m_current) found = it;
        }
        m_tree->expandAll();

        QTreeWidgetItem *target = found ? found : first;
        if (!target)
            return;
        m_tree->setCurrentItem(target);
        const QString id = target->data(0, Qt::UserRole).toString();
        if (id != m_current) {                       // le composant précédent n'existe plus ici
            m_current = id;
            if (onPick)
                onPick(id);
        }
    }

    QLabel *m_title = nullptr;
    QLineEdit *m_search = nullptr;
    QCheckBox *m_all = nullptr;
    QTreeWidget *m_tree = nullptr;
    int m_module = ModAll;
    QString m_current;
};