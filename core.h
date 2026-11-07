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

/* Un fil : ses deux extrémités */
void schema_add_wire(Schema *s, int x1, int y1, int x2, int y2);

/* Calcule les nets. Retourne le nombre de nets. */
int  schema_compute_nets(Schema *s);

/* Net d'une broche, ou -1 si elle n'est reliée à rien, ou -2 si inconnue */
int  schema_pin_net(const Schema *s, int comp, int pin);

#ifdef __cplusplus
}
#endif
#endif