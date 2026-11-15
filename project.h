#pragma once
#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

// Modèles de feuille (cadres), sur le principe des modèles de folio d'un logiciel de schéma électrotechnique
enum SheetFrame {
    FrameNormal = 0,        // feuille blanche, sans cadre ni cartouche
    FrameCadre = 1,         // cadre avec repères de colonnes et de lignes
    FrameCartouche = 2,     // cadre + cartouche
    FrameLogo = 3,          // cadre + cartouche avec logo
    FrameSimple = 4         // modèle 1 - simple : filet et bandeau de titre
};

struct ProjectInfo {
    QString paper = "A4";                 // "A4", "A3" ou "A2"
    bool portrait = false;                // orientation de la feuille
    int frame = FrameCartouche;           // modèle de cadre (SheetFrame)
    QByteArray logo;                      // image du logo (PNG), pour le cartouche avec logo
    QString company, address, title, author;
    QString revision = "A";
    QString created = QDate::currentDate().toString(Qt::ISODate);
    QString modified = QDate::currentDate().toString(Qt::ISODate);
};

// Liste des modèles proposés : libellé, format, orientation, cadre, nombre de folios créés
struct SheetModel {
    const char *name;
    const char *paper;
    bool portrait;
    int frame;
    int folios;
};

inline const QList<SheetModel> &sheetModels()
{
    static const QList<SheetModel> m = {
        {"Cadre A2 - Paysage", "A2", false, FrameCadre, 1},
        {"Cadre A2 - Portrait", "A2", true, FrameCadre, 1},
        {"Cadre A2 + Cartouche - Paysage", "A2", false, FrameCartouche, 1},
        {"Cadre A2 + Cartouche - Portrait", "A2", true, FrameCartouche, 1},
        {"Cadre A3 - Paysage", "A3", false, FrameCadre, 1},
        {"Cadre A3 - Portrait", "A3", true, FrameCadre, 1},
        {"Cadre A3 + Cartouche - Paysage", "A3", false, FrameCartouche, 1},
        {"Cadre A3 + Cartouche - Portrait", "A3", true, FrameCartouche, 1},
        {"Cadre A4 - Paysage", "A4", false, FrameCadre, 1},
        {"Cadre A4 - Portrait", "A4", true, FrameCadre, 1},
        {"Cadre A4 + Cartouche - Paysage", "A4", false, FrameCartouche, 1},
        {"Cadre A4 + Cartouche - Portrait", "A4", true, FrameCartouche, 1},
        {"Cadre A4 + Cartouche avec logo - Portrait - 2 folios", "A4", true, FrameLogo, 2},
        {"Cadre A4 + Cartouche avec logo - Portrait", "A4", true, FrameLogo, 1},
        {"Cadre A4 + Cartouche avec logo - Paysage", "A4", false, FrameLogo, 1},
        {"Modèle 1 - Simple - Paysage", "A4", false, FrameSimple, 1},
        {"Modèle 1 - Simple - Portrait", "A4", true, FrameSimple, 1},
        {"Normal (feuille A4 sans cadre)", "A4", false, FrameNormal, 1},
    };
    return m;
}

// rang du modèle qui correspond à un projet (-1 : combinaison personnalisée)
inline int sheetModelOf(const ProjectInfo &pi)
{
    const QList<SheetModel> &m = sheetModels();
    for (int i = 0; i < m.size(); ++i)
        if (pi.paper == m[i].paper && pi.portrait == m[i].portrait && pi.frame == m[i].frame && m[i].folios == 1)
            return i;
    return -1;
}

struct Folio {
    QString title;
    QByteArray state;                     // contenu du folio (JSON)
    QList<QByteArray> undo, redo;
};
