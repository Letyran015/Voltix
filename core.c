#include "core.h"
#include <stdlib.h>

typedef struct { int x, y; int comp, pin; int wire; } Node;   /* comp = -1 : bout de fil */
typedef struct { int x1, y1, x2, y2; int wire; int rep; } Seg; /* rep = nœud représentant le fil */

struct Schema {
    Node *nodes;  int count, cap;
    int  *links;  int link_count, link_cap;      /* paires de nœuds (fils) */
    Seg  *segs;   int seg_count, seg_cap;
    int   wire_count;
    int  *parent;
    int  *net;
};

Schema *schema_new(void)
{
    return (Schema *)calloc(1, sizeof(Schema));
}

void schema_free(Schema *s)
{
    if (!s) return;
    free(s->nodes); free(s->links); free(s->segs);
    free(s->parent); free(s->net);
    free(s);
}

static int add_node(Schema *s, int x, int y, int comp, int pin, int wire)
{
    if (s->count == s->cap) {
        int ncap = s->cap ? s->cap * 2 : 64;
        Node *p = (Node *)realloc(s->nodes, (size_t)ncap * sizeof(Node));
        if (!p) return -1;
        s->nodes = p; s->cap = ncap;
    }
    s->nodes[s->count].x = x;       s->nodes[s->count].y = y;
    s->nodes[s->count].comp = comp; s->nodes[s->count].pin = pin;
    s->nodes[s->count].wire = wire;
    return s->count++;
}

static int add_seg(Schema *s, int x1, int y1, int x2, int y2, int wire, int rep)
{
    if (s->seg_count == s->seg_cap) {
        int ncap = s->seg_cap ? s->seg_cap * 2 : 64;
        Seg *p = (Seg *)realloc(s->segs, (size_t)ncap * sizeof(Seg));
        if (!p) return 0;
        s->segs = p; s->seg_cap = ncap;
    }
    s->segs[s->seg_count].x1 = x1; s->segs[s->seg_count].y1 = y1;
    s->segs[s->seg_count].x2 = x2; s->segs[s->seg_count].y2 = y2;
    s->segs[s->seg_count].wire = wire; s->segs[s->seg_count].rep = rep;
    s->seg_count++;
    return 1;
}

void schema_add_pin(Schema *s, int comp, int pin, int x, int y)
{
    add_node(s, x, y, comp, pin, -1);
}

void schema_add_wire(Schema *s, const int *xy, int npts)
{
    int a, b, k, wid;
    if (npts < 2) return;
    wid = s->wire_count++;
    a = add_node(s, xy[0], xy[1], -1, -1, wid);
    b = add_node(s, xy[2 * (npts - 1)], xy[2 * (npts - 1) + 1], -1, -1, wid);
    if (a < 0 || b < 0) return;

    if (s->link_count + 2 > s->link_cap) {
        int ncap = s->link_cap ? s->link_cap * 2 : 64;
        int *p = (int *)realloc(s->links, (size_t)ncap * sizeof(int));
        if (!p) return;
        s->links = p; s->link_cap = ncap;
    }
    s->links[s->link_count++] = a;
    s->links[s->link_count++] = b;

    for (k = 0; k + 1 < npts; k++)
        add_seg(s, xy[2 * k], xy[2 * k + 1], xy[2 * k + 2], xy[2 * k + 3], wid, a);
}

/* ---- union-find ---- */
static int find(int *parent, int i)
{
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];   /* compression de chemin */
        i = parent[i];
    }
    return i;
}

static void unite(int *parent, int a, int b)
{
    int ra = find(parent, a), rb = find(parent, b);
    if (ra != rb) parent[rb] = ra;
}

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

/* le point (px,py) est-il sur le segment ? */
static int on_seg(const Seg *g, int px, int py)
{
    long long cross = (long long)(g->x2 - g->x1) * (py - g->y1)
    - (long long)(g->y2 - g->y1) * (px - g->x1);
    if (cross != 0) return 0;
    return px >= imin(g->x1, g->x2) && px <= imax(g->x1, g->x2)
           && py >= imin(g->y1, g->y2) && py <= imax(g->y1, g->y2);
}

int schema_compute_nets(Schema *s)
{
    int n = s->count, i, j, k, next = 0;
    free(s->parent); free(s->net);
    s->parent = NULL; s->net = NULL;
    if (n == 0) return 0;

    s->parent = (int *)malloc((size_t)n * sizeof(int));
    s->net    = (int *)malloc((size_t)n * sizeof(int));
    int *size  = (int *)calloc((size_t)n, sizeof(int));
    int *label = (int *)malloc((size_t)n * sizeof(int));
    if (!s->parent || !s->net || !size || !label) {
        free(size); free(label);
        return 0;
    }

    for (i = 0; i < n; i++) { s->parent[i] = i; label[i] = -1; }

    /* 1. les deux bouts d'un fil sont au même potentiel */
    for (i = 0; i + 1 < s->link_count; i += 2)
        unite(s->parent, s->links[i], s->links[i + 1]);

    /* 2. deux points au même endroit sont au même potentiel */
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (s->nodes[i].x == s->nodes[j].x && s->nodes[i].y == s->nodes[j].y)
                unite(s->parent, i, j);

    /* 3. jonctions en T : un point posé sur un autre fil est relié à ce fil.
          Deux fils qui se croisent sans extrémité commune ne sont PAS reliés. */
    for (i = 0; i < n; i++)
        for (k = 0; k < s->seg_count; k++)
            if (s->nodes[i].wire != s->segs[k].wire
                && on_seg(&s->segs[k], s->nodes[i].x, s->nodes[i].y))
                unite(s->parent, i, s->segs[k].rep);

    /* 4. numérotation des nets (un groupe d'un seul point = non connecté) */
    for (i = 0; i < n; i++) size[find(s->parent, i)]++;
    for (i = 0; i < n; i++) {
        int r = find(s->parent, i);
        if (size[r] > 1) {
            if (label[r] < 0) label[r] = next++;
            s->net[i] = label[r];
        } else {
            s->net[i] = -1;
        }
    }
    free(size); free(label);
    return next;
}

int schema_pin_net(const Schema *s, int comp, int pin)
{
    int i;
    if (!s->net) return -2;
    for (i = 0; i < s->count; i++)
        if (s->nodes[i].comp == comp && s->nodes[i].pin == pin)
            return s->net[i];
    return -2;
}