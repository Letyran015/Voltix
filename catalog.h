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

// appareils à plusieurs pôles : identifiant de la forme « mcb_3p »
struct PoleType {
    QString id, title, prefix, group;
    int module;
    int mask;         // bit n = existe en n pôles
};

inline const QList<PoleType> &poleTypes()
{
    static const QList<PoleType> t = {
                                       {"mcb",        "Disjoncteur magnétothermique",   "Q",  "Protection",                   ModElectrotech, 0x1E},
                                       {"mpcb",       "Disjoncteur moteur",             "Q",  "Protection",                   ModElectrotech, 0x08},
                                       {"rcd",        "Interrupteur différentiel",      "Q",  "Protection",                   ModElectrotech, 0x14},
                                       {"rcbo",       "Disjoncteur différentiel",       "Q",  "Protection",                   ModElectrotech, 0x14},
                                       {"surge",      "Parafoudre",                     "F",  "Protection",                   ModElectrotech, 0x1E},
                                       {"fuseholder", "Porte-fusible",                  "F",  "Protection",                   ModElectrotech, 0x0E},
                                       {"fusedisc",   "Sectionneur porte-fusible",      "Q",  "Protection",                   ModElectrotech, 0x0E},
                                       {"disc",       "Sectionneur",                    "Q",  "Sectionnement et commutation", ModElectrotech, 0x1E},
                                       {"swdisc",     "Interrupteur-sectionneur",       "Q",  "Sectionnement et commutation", ModElectrotech, 0x1E},
                                       {"sw",         "Interrupteur",                   "Q",  "Sectionnement et commutation", ModElectrotech, 0x1E},
                                       {"contactor",  "Contacteur (pôles de puissance)","KM", "Contacteurs et relais",        ModElectrotech, 0x1E},
                                       };
    return t;
}

inline const PoleType *poleType(const QString &id)
{
    for (const PoleType &t : poleTypes())
        if (t.id == id)
            return &t;
    return nullptr;
}

inline const QList<CatalogEntry> &catalog()
{
    static const QList<CatalogEntry> c = [] {
        QList<CatalogEntry> v = {
                                  {"resistor",    "Résistance",        "Composants passifs", ModAll},
                                  {"capacitor",   "Condensateur",      "Composants passifs", ModAll},
                                  {"inductor",    "Inductance",        "Composants passifs", ModAll},
                                  {"transformer", "Transformateur",    "Composants passifs", ModAll},

                                  {"vsource", "Source DC",         "Sources et repères", ModAll},
                                  {"vsin",    "Source sinus",      "Sources et repères", ModAll},
                                  {"vpulse",  "Source impulsion",  "Sources et repères", ModElectronics | ModElectromag},
                                  {"ground",  "Masse",             "Sources et repères", ModAll},
                                  {"label",   "Étiquette de nœud", "Sources et repères", ModAll},

                                  {"diode", "Diode",          "Semi-conducteurs", ModElectronics},
                                  {"npn",   "Transistor NPN", "Semi-conducteurs", ModElectronics},
                                  {"pnp",   "Transistor PNP", "Semi-conducteurs", ModElectronics},
                                  {"opamp", "Ampli-op",       "Semi-conducteurs", ModElectronics},

                                  {"grid3", "Réseau triphasé",            "Alimentation et terre", ModElectrotech},
                                  {"earth", "Terre (PE)",                 "Alimentation et terre", ModElectrotech},
                                  {"psu",   "Alimentation AC/DC",         "Alimentation et terre", ModElectrotech},
                                  };
        auto addPoles = [&v](const QString &group) {
            for (const PoleType &t : poleTypes())
                if (t.group == group)
                    for (int n = 1; n <= 4; ++n)
                        if (t.mask & (1 << n))
                            v.append({QString("%1_%2p").arg(t.id).arg(n),
                                      QString("%1 %2P").arg(t.title).arg(n), t.group, t.module});
        };

        addPoles("Protection");
        v.append({"fuse",    "Fusible",                          "Protection", ModElectrotech});
        v.append({"thermal3", "Relais thermique (puissance)",    "Protection", ModElectrotech});

        addPoles("Sectionnement et commutation");

        addPoles("Contacteurs et relais");
        const QString g = "Contacteurs et relais";
        v.append({"coil",          "Bobine de contacteur",       g, ModElectrotech});
        v.append({"relay",         "Bobine de relais",           g, ModElectrotech});
        v.append({"timer_coil",    "Relais temporisé (bobine)",  g, ModElectrotech});
        v.append({"contact_no",    "Contact NO",                 g, ModElectrotech});
        v.append({"contact_nc",    "Contact NC",                 g, ModElectrotech});
        v.append({"contact_co",    "Contact inverseur",          g, ModElectrotech});
        v.append({"contact_td_no", "Contact NO temporisé",       g, ModElectrotech});
        v.append({"contact_td_nc", "Contact NC temporisé",       g, ModElectrotech});
        v.append({"thermal_nc",    "Contact thermique (NC)",     g, ModElectrotech});

        const QString m = "Moteurs et charges";
        v.append({"motor3",  "Moteur triphasé",             m, ModElectrotech});
        v.append({"motor1",  "Moteur monophasé",            m, ModElectrotech});
        v.append({"motordc", "Moteur à courant continu",    m, ModElectrotech});
        v.append({"valve",   "Électrovanne",                m, ModElectrotech});
        v.append({"heater",  "Résistance chauffante",       m, ModElectrotech});

        v.append({"plc", "Automate programmable", "Automate", ModElectrotech});

        v.append({"terminal", "Bornier", "Raccordement", ModElectrotech});

        const QString s = "Capteurs";
        v.append({"limit_no", "Fin de course NO",          s, ModElectrotech});
        v.append({"limit_nc", "Fin de course NC",          s, ModElectrotech});
        v.append({"level_no", "Détecteur de niveau",       s, ModElectrotech});
        v.append({"press_no", "Pressostat",                s, ModElectrotech});
        v.append({"therm_no", "Thermostat",                s, ModElectrotech});
        v.append({"sens_ind", "Capteur inductif (3 fils)", s, ModElectrotech});
        v.append({"sens_cap", "Capteur capacitif (3 fils)",s, ModElectrotech});
        v.append({"sens_pho", "Capteur photoélectrique (3 fils)", s, ModElectrotech});

        const QString k = "Commande et signalisation";
        v.append({"pushbutton_no", "Bouton poussoir NO",   k, ModElectrotech});
        v.append({"pushbutton_nc", "Bouton poussoir NC",   k, ModElectrotech});
        v.append({"estop",         "Arrêt d'urgence",      k, ModElectrotech});
        v.append({"selector",      "Commutateur à clé / sélecteur", k, ModElectrotech});
        v.append({"lamp",          "Voyant",               k, ModElectrotech});
        v.append({"buzzer",        "Avertisseur sonore",   k, ModElectrotech});
        return v;
    }();
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