#ifndef CORE_H
#define CORE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Schema Schema;

Schema *schema_new(void);
void    schema_free(Schema *s);

/* Une broche : numéro de composant, numéro de broche, position (grille) */
void schema_add_pin(Schema *s, int comp, int pin, int x, int y);

/* Un fil : polyligne xy = x0,y0,x1,y1,... (npts points). */
void schema_add_wire(Schema *s, const int *xy, int npts);

/* Relie deux broches d'un même composant (bornier, par exemple). */
void schema_connect_pins(Schema *s, int comp, int pin_a, int pin_b);

/* Calcule les nets. Retourne le nombre de nets. */
int  schema_compute_nets(Schema *s);

/* Net d'une broche, ou -1 si elle n'est reliée à rien, ou -2 si inconnue */
int  schema_pin_net(const Schema *s, int comp, int pin);

/* Net d'un fil (numéro dans l'ordre d'ajout), ou -2 si inconnu */
int  schema_wire_net(const Schema *s, int wire);

#ifdef __cplusplus
}
#endif
#endif