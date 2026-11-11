#pragma once
#include <QList>
#include <QString>

enum Module { ModElectronics = 1, ModElectrotech = 2, ModElectromag = 4, ModAll = 7 };

struct CatalogEntry {
    QString id;       // identifiant utilisé par makeSymbol()
    QString name;     // nom affiché
    QString group;    // groupe dans la bibliothèque
    int modules;      // modules où il apparaît
};

inline const QList<CatalogEntry> &catalog()
{
    static const QList<CatalogEntry> c = {
                                           {"resistor",  "Résistance",   "Composants passifs", ModAll},
                                           {"capacitor", "Condensateur", "Composants passifs", ModAll},
                                           {"inductor",  "Inductance",   "Composants passifs", ModAll},
                                           {"transformer", "Transformateur", "Composants passifs", ModAll},

                                           {"vsource", "Source DC",         "Sources et repères", ModAll},
                                           {"vsin",    "Source sinus",      "Sources et repères", ModAll},
                                           {"vpulse",  "Source impulsion",  "Sources et repères", ModElectronics | ModElectromag},
                                           {"ground",  "Masse",             "Sources et repères", ModAll},
                                           {"label",   "Étiquette de nœud", "Sources et repères", ModAll},

                                           {"diode", "Diode",            "Semi-conducteurs", ModElectronics},
                                           {"npn",   "Transistor NPN",   "Semi-conducteurs", ModElectronics},
                                           {"pnp",   "Transistor PNP",   "Semi-conducteurs", ModElectronics},
                                           {"opamp", "Ampli-op",         "Semi-conducteurs", ModElectronics},

                                           {"grid3",         "Réseau triphasé",           "Puissance", ModElectrotech},
                                           {"disconnector3", "Sectionneur tripolaire",    "Puissance", ModElectrotech},
                                           {"breaker3",      "Disjoncteur tripolaire",    "Puissance", ModElectrotech},
                                           {"contactor3",    "Contacteur (pôles)",        "Puissance", ModElectrotech},
                                           {"fuse",          "Fusible",                   "Puissance", ModElectrotech},
                                           {"thermal3",      "Relais thermique",          "Puissance", ModElectrotech},
                                           {"motor3",        "Moteur triphasé",           "Puissance", ModElectrotech},

                                           {"coil",          "Bobine de contacteur",      "Commande et signalisation", ModElectrotech},
                                           {"relay",         "Bobine de relais",          "Commande et signalisation", ModElectrotech},
                                           {"contact_no",    "Contact NO",                "Commande et signalisation", ModElectrotech},
                                           {"contact_nc",    "Contact NC",                "Commande et signalisation", ModElectrotech},
                                           {"pushbutton_no", "Bouton poussoir NO",        "Commande et signalisation", ModElectrotech},
                                           {"pushbutton_nc", "Bouton poussoir NC",        "Commande et signalisation", ModElectrotech},
                                           {"thermal_nc",    "Contact thermique (NC)",    "Commande et signalisation", ModElectrotech},
                                           {"lamp",          "Voyant",                    "Commande et signalisation", ModElectrotech},
                                           };
    return c;
}

inline QString entryName(const QString &id)
{
    for (const CatalogEntry &e : catalog())
        if (e.id == id)
            return e.name;
    return id;
}

inline QString moduleTitle(int m)
{
    switch (m) {
    case ModElectronics: return "Électronique";
    case ModElectrotech: return "Électrotechnique";
    case ModElectromag:  return "Électromagnétique";
    default:             return "Tous les modules";
    }
}