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
                                  {"rheostat", "Résistance variable (rhéostat)", "Composants passifs", ModAll},
                                  {"pot",     "Potentiomètre",              "Composants passifs", ModAll},
                                  {"ntc",     "Thermistance CTN",           "Composants passifs", ModAll},
                                  {"ldr",      "Photorésistance (LDR)",          "Composants passifs", ModAll},
                                  {"inductor",    "Inductance",        "Composants passifs", ModAll},
                                  {"ind_fe",   "Inductance à noyau de fer",      "Composants passifs", ModAll},
                                  {"ind_var",  "Inductance variable",            "Composants passifs", ModAll},
                                  {"transformer", "Transformateur",    "Composants passifs", ModAll},
                                  {"transformer_ip", "Transformateur (secondaire en phase)",          "Composants passifs", ModAll},
                                  {"transformer_op", "Transformateur (secondaire en opposition de phase)", "Composants passifs", ModAll},
                                  {"capacitor",   "Condensateur",      "Composants passifs", ModAll},
                                  {"cap_pol",  "Condensateur polarisé",          "Composants passifs", ModAll},
                                  {"cap_var",  "Condensateur variable",          "Composants passifs", ModAll},
                                  {"quartz",  "Quartz",       "Composants passifs", ModElectronics},
                                  {"speaker", "Haut-parleur", "Composants passifs", ModAll},
                                  {"bell",     "Sonnerie",                       "Composants passifs", ModAll},
                                  {"bulb",     "Lampe",                          "Composants passifs", ModAll},

                                  {"vsource", "Source de tension DC", "Sources et repères", ModAll},
                                  {"vsin",    "Source sinus",      "Sources et repères", ModAll},
                                  {"vpulse",  "Source impulsion",  "Sources et repères", ModElectronics | ModElectromag},
                                  {"triangle", "Source triangle",  "Sources et repères", ModAll},
                                  {"ground",  "Masse",             "Sources et repères", ModAll},

                                  {"vmeter",  "Voltmètre",         "Mesure", ModAll},
                                  {"ammeter", "Ampèremètre",       "Mesure", ModAll},

                                  {"diode", "Diode",          "Semi-conducteurs", ModElectronics},
                                  {"npn",   "Transistor NPN", "Semi-conducteurs", ModElectronics},
                                  {"pnp",   "Transistor PNP", "Semi-conducteurs", ModElectronics},
                                  {"opamp", "Ampli-op",       "Semi-conducteurs", ModElectronics},
                                  {"zener",    "Diode Zener",     "Semi-conducteurs", ModElectronics},
                                  {"led",      "LED",             "Semi-conducteurs", ModElectronics},
                                  {"schottky", "Diode Schottky",  "Semi-conducteurs", ModElectronics},
                                  {"nmos",     "MOSFET canal N",  "Semi-conducteurs", ModElectronics},
                                  {"pmos",     "MOSFET canal P",  "Semi-conducteurs", ModElectronics},
                                  {"njfet",    "JFET canal N",    "Semi-conducteurs", ModElectronics},
                                  {"pjfet",    "JFET canal P",    "Semi-conducteurs", ModElectronics},
                                  {"thyristor","Thyristor",       "Semi-conducteurs", ModAll},
                                  {"triac",    "Triac",           "Semi-conducteurs", ModAll},
                                  {"diac",     "Diac",            "Semi-conducteurs", ModAll},
                                  {"bridge",   "Pont redresseur", "Semi-conducteurs", ModAll},
                                  {"photodiode","Photodiode",     "Semi-conducteurs", ModElectronics},
                                  {"tunnel",   "Diode tunnel",    "Semi-conducteurs", ModElectronics},


                                  {"lsrc",   "Entrée logique (0/1)", "Entrées / sorties logiques", ModElectronics},
                                  {"clock",  "Horloge",              "Entrées / sorties logiques", ModElectronics},
                                  {"lprobe", "Sonde logique",        "Entrées / sorties logiques", ModElectronics},



                                  {"summer",     "Sommateur",         "Blocs de commande", ModAll},
                                  {"gain",       "Gain",              "Blocs de commande", ModAll},
                                  {"comparator", "Comparateur",       "Blocs de commande", ModAll},
                                  {"vswitch",    "Interrupteur commandé", "Blocs de commande", ModAll},

                                  {"timer555", "Temporisateur 555",   "Circuits intégrés", ModElectronics},
                                  {"tl431",    "TL431",               "Circuits intégrés", ModElectronics},
                                  {"opto",     "Optocoupleur",        "Circuits intégrés", ModElectronics},

                                  {"igbt",     "IGBT (interrupteur commandé)", "Semi-conducteurs", ModAll},

                                  {"isource", "Source de courant DC",       "Sources et repères", ModAll},
                                  {"vcvs",    "Source tension / tension",   "Sources commandées", ModElectronics},
                                  {"vccs",    "Source courant / tension",   "Sources commandées", ModElectronics},
                                  {"cccs",    "Source courant / courant",   "Sources commandées", ModElectronics},
                                  {"ccvs",    "Source tension / courant",   "Sources commandées", ModElectronics},

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