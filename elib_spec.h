#pragma once
// Description d'un symbole de la bibliothèque d'électrotechnique (voir elib.h / elib_data.h).
struct LibSpec {
    const char *id;
    const char *name;
    const char *group;    // dossier de la bibliothèque
    const char *prefix;   // préfixe du repère (KM, Q, F...)
    const char *pins;     // "x,y;x,y;..."
    const char *ops;      // tracés séparés par « ; »
    const char *props;    // "nom|valeur|unité|description|affiché;..."
    const char *role;     // "coil", "contact", "power", "thermal", "thcontact" ou vide
};
