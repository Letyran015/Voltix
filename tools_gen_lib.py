# -*- coding: utf-8 -*-
# Voltix - generateur de elib_data.h
# Symboles redessines a partir des formes normalisees (CEI 60617) telles qu'elles apparaissent
# dans les dossiers ouverts des captures. Unites : pas de grille 20, poles au pas de 40.
import sys, math
S = []
GROUP = [""]
def G(name): GROUP[0] = name
def add(id, name, prefix, pins, ops, props="", role=""):
    ps = ";".join("%g,%g" % p for p in pins)
    flat = []
    for o in ops:
        if isinstance(o, (list, tuple)): flat += list(o)
        elif o: flat.append(o)
    S.append(("e_" + id, name, GROUP[0], prefix, ps, ";".join(flat), props or "", role))
def n(v): return "%g" % round(v, 2)
def L(*a): return "L " + " ".join(n(v) for v in a)
def D(*a): return "D " + " ".join(n(v) for v in a)
def R(*a): return "R " + " ".join(n(v) for v in a)
def RF(*a): return "RF " + " ".join(n(v) for v in a)
def RR(*a): return "RR " + " ".join(n(v) for v in a)
def C(x, y, r): return "C %s %s %s" % (n(x), n(y), n(r))
def F(x, y, r): return "F %s %s %s" % (n(x), n(y), n(r))
def E(x, y, r, col): return "E %s %s %s %s" % (n(x), n(y), n(r), col)
def A(x, y, w, h, s, sp): return "A " + " ".join(n(v) for v in (x, y, w, h, s, sp))
def P(*pts): return "P " + " ".join(n(v) for v in pts)
def DP(*pts): return "DP " + " ".join(n(v) for v in pts)
def Q(*pts): return "Q " + " ".join(n(v) for v in pts)
def K(*pts): return "K " + " ".join(n(v) for v in pts)
def W(w): return "W %s" % n(w)
def O(c="-"): return "O %s" % c
def _t(op, x, y, px, t): return "%s %s %s %d %s" % (op, n(x), n(y), px, str(t).replace(" ", "_"))
def T(x, y, px, t): return _t("T", x, y, px, t)
def TB(x, y, px, t): return _t("B", x, y, px, t)
def TL(x, y, px, t): return _t("TL", x, y, px, t)
def TR(x, y, px, t): return _t("TR", x, y, px, t)
def TI(x, y, px, t): return _t("I", x, y, px, t)
def prop(nm, v, u="", d="", show=False): return "%s|%s|%s|%s|%d" % (nm, v, u, d or nm, 1 if show else 0)
def props(*p): return ";".join(p)
def bornes(v): return prop("Bornes", v, "", "Repères des bornes")
def bob(v="KM1"): return prop("Bobine", v, "", "Repère de la bobine ou de l'appareil associé", True)
def xs_of(k, pitch=40): return [(-(k - 1) * pitch / 2.0 + pitch * i) for i in range(k)]
def rotrect(cx, cy, length, width, dx, dy):
    """rectangle centre (cx,cy) aligne sur la direction (dx,dy)"""
    m = math.hypot(dx, dy); ux, uy = dx / m, dy / m; vx, vy = -uy, ux
    a, b = length / 2.0, width / 2.0
    return Q(cx - ux * a - vx * b, cy - uy * a - vy * b, cx + ux * a - vx * b, cy + uy * a - vy * b,
             cx + ux * a + vx * b, cy + uy * a + vy * b, cx - ux * a + vx * b, cy - uy * a + vy * b)

# ------------------------------------------------------------------ contacts
def NO(x=0): return [L(x, -40, x, -16), L(x, 26, x - 16, -16), L(x, 26, x, 40)]
def NC(x=0): return [L(x, -40, x, -18), L(x, -18, x + 21, -18), L(x, 28, x + 13, -23), L(x, 28, x, 40)]
def bx(nc, x=0, y=0):                      # abscisse de la lame a la hauteur y
    return x + 13 * (28 - y) / 51.0 if nc else x - 16 * (26 - y) / 42.0
def lab(x=0, a="$b0", b="$b1"): return [TL(x + 5, -30, 8, a), TL(x + 5, 32, 8, b)]
def CT(nc, x=0): return NC(x) if nc else NO(x)
def CO(x=0, lab3=True):                    # inverseur : commun en bas, NF a gauche, NO a droite
    o = [L(x - 20, -40, x - 20, -18), L(x - 20, -18, x - 5, -18), L(x, -40, x, -22),
         L(x, 28, x - 9, -20), L(x, 28, x, 40)]
    if lab3: o += [TL(x - 16, -30, 8, "$b1"), TL(x + 4, -30, 8, "$b2"), TL(x + 5, 32, 8, "$b0")]
    return o
def co_pins(x=0): return [(x, 40), (x - 20, -40), (x, -40)]
def tempo(nc, kind, x=0):
    """kind 't' = travail : arc bombe vers le contact ; 'r' = repos : arc bombe a l'oppose"""
    o = []
    e1, e2 = bx(nc, x, -3), bx(nc, x, 3)
    if kind in ("t", "b"):
        ax = x - 30
        o += [A(ax - 20, -11, 20, 22, -62, 124), L(ax, -3, e1, -3), L(ax, 3, e2, 3)]
    if kind in ("r", "b"):
        ax = x - 36 if kind == "r" else x - 34
        o += [A(ax, -11, 20, 22, 118, 124), L(ax, -3, e1, -3), L(ax, 3, e2, 3)]
    return o
def thermal_mark(nc, x=0):
    b = bx(nc, x, 0); tx = x + 22 if not nc else x + 26
    return [D(b + 2, 0, tx, 0), P(tx + 7, -9, tx + 7, -4, tx, -4, tx, 4, tx + 7, 4, tx + 7, 9)]

def link(nc, lx, x=0, y=0): return [D(lx, y, bx(nc, x, y) - 1, y)]
def actuator(kind, nc, x=0):
    """organe de commande place a gauche de la lame"""
    lx = x - 30
    o = []
    if kind == "E":                         # poussoir
        o = [L(lx - 8, -7, lx - 8, 7), L(lx - 8, -7, lx - 1, -7), L(lx - 8, 7, lx - 1, 7), L(lx - 8, 0, lx - 3, 0)] + link(nc, lx + 1, x)
    elif kind == "tir":                     # tirette
        o = [P(lx - 8, -7, lx - 2, -7, lx - 2, 7, lx - 8, 7)] + link(nc, lx - 2, x)
    elif kind == "rot":                     # bouton tournant
        o = [P(lx - 10, 6, lx - 4, 6, lx - 4, -6, lx + 2, -6)] + link(nc, lx - 4, x)
    elif kind == "mush":                    # coup de poing
        o = [A(lx - 26, -10, 20, 20, 90, 180), L(lx - 16, -10, lx - 16, 10), L(lx - 16, 0, lx - 9, 0),
             P(lx - 9, -4, lx - 3, -4, lx - 3, 4, lx - 9, 4)] + link(nc, lx - 3, x)
    elif kind == "mushkey":
        o = actuator("mush", nc, x) + [C(lx - 6, 12, 3), Q(lx - 8, 15, lx - 4, 15, lx - 2, 22, lx - 10, 22)]
    elif kind == "cable":                   # arret d'urgence a cable
        o = [C(lx - 24, 0, 2), P(lx - 22, 0, lx - 19, -3, lx - 16, 3, lx - 13, -3, lx - 10, 3, lx - 7, -3, lx - 4, 0)] + link(nc, lx - 2, x)
    elif kind == "lum":                     # poussoir lumineux : E a gauche, voyant a droite de la lame
        o = actuator("E", nc, x)
        b = bx(nc, x, 0); cx = b + (14 if not nc else 16)
        o += [D(b + 2, 0, cx - 6, 0), C(cx, 0, 6), L(cx - 4.2, -4.2, cx + 4.2, 4.2), L(cx - 4.2, 4.2, cx + 4.2, -4.2)]
    elif kind == "key":                     # serrure
        o = [C(lx - 12, -4, 4), Q(lx - 14, -1, lx - 10, -1, lx - 8, 9, lx - 16, 9), P(lx - 8, 0, lx - 2, 0)] + link(nc, lx, x)
    elif kind == "sel":                     # selecteur a positions maintenues
        o = [P(lx - 18, 7, lx - 13, 7, lx - 13, -5, lx - 8, -5), L(lx - 13, 0, lx - 6, 0), P(lx - 6, 0, lx - 2, 8, lx + 2, 0)] + link(nc, lx + 4, x)
    elif kind == "selkey":
        o = actuator("sel", nc, x) + [C(lx - 16, -16, 3), Q(lx - 18, -13, lx - 14, -13, lx - 12, -7, lx - 20, -7)]
    elif kind == "ped":                     # pedale
        o = [Q(lx - 16, -7, lx - 16, 7, lx - 4, 0), C(lx - 2, 0, 1.5)] + link(nc, lx + 1, x)
    elif kind == "wheel":                   # volant
        o = [C(lx - 8, 0, 7), C(lx - 8, 0, 2), L(lx - 8, -2, lx - 8, -7), L(lx - 6.3, 1, lx - 2, 3.5), L(lx - 9.7, 1, lx - 14, 3.5)] + link(nc, lx, x)
    elif kind == "roll":                    # galet (fin de course)
        o = [C(lx - 6, 0, 5)] + link(nc, lx, x)
    elif kind == "rollwall":
        o = [C(lx - 6, 0, 5), L(lx - 14, -16, lx - 14, 16)] + [L(lx - 14, y, lx - 22, y + 6) for y in (-14, -8, -2, 4, 10)] + link(nc, lx, x)
    elif kind == "cam":                     # came
        o = [A(lx - 18, -7, 14, 14, 30, 300), L(lx - 11, 0, lx - 5, -3.5), L(lx - 11, 0, lx - 5, 3.5), C(lx - 2, 0, 2)] + link(nc, lx + 1, x)
    elif kind == "lev":                     # niveau (flotteur a tige)
        o = [C(lx - 4, -9, 2.5), C(lx - 4, 9, 2.5), P(lx - 5, -7, lx - 12, 0, lx - 5, 7), L(lx - 12, 0, lx - 2, 0)] + link(nc, lx, x)
    elif kind == "oval":                    # detection de proximite (aimant / verin)
        o = [RR(lx - 22, -6, 20, 12)] + link(nc, lx, x)
    elif kind == "D":
        o = [P(lx - 12, -7, lx - 12, 7), A(lx - 19, -7, 14, 14, 90, -180), L(lx - 12, -7, lx - 12, 7)] + link(nc, lx - 4, x)
    elif kind == "float":                   # flotteur
        o = [R(lx - 18, -9, 18, 18), A(lx - 14, 0, 10, 6, 0, 360), L(lx - 9, 0, lx - 9, -5), L(lx - 12, -5, lx - 6, -5)] + link(nc, lx + 2, x)
    elif kind == "P":                       # pressostat
        o = [R(lx - 18, -9, 18, 18), T(lx - 9, 0, 10, "P")] + link(nc, lx + 2, x)
    elif kind == "temp":                    # thermostat
        o = [C(lx - 8, 0, 8), L(lx - 8, -8, lx - 8, 8), T(lx - 8, -15, 8, "T°")] + link(nc, lx + 2, x)
    elif kind == "clock":
        o = [C(lx - 10, 0, 10), L(lx - 10, 0, lx - 10, -7), L(lx - 10, 0, lx - 4, 0)] + link(nc, lx + 2, x)
    elif kind == "I>":
        o = [R(lx - 22, -11, 22, 22), TI(lx - 11, 0, 13, "I>")] + link(nc, lx + 2, x)
    elif kind == "box":
        o = [R(lx - 20, -11, 20, 22), L(lx - 10, -11, lx - 10, -18), L(lx - 10, 11, lx - 10, 18)] + link(nc, lx + 2, x)
    return o

# ------------------------------------------------------------------ bobines
def coil(w=50, h=28, a="$b0", b="$b1", extra=None):
    o = [L(0, -40, 0, -h / 2.0), L(0, h / 2.0, 0, 40), R(-w / 2.0, -h / 2.0, w, h)]
    if a: o += [TL(5, -30, 8, a), TL(5, 32, 8, b)]
    return o + (extra or [])
def coil_side(kind):                         # bobine avec case laterale a droite
    o = coil(50, 28)
    x0 = 25; w = 12
    o += [R(x0, -14, w, 28)]
    if kind == "black": o += [RF(x0, -14, w, 28)]
    elif kind == "cross": o += [L(x0, -14, x0 + w, 14), L(x0, 14, x0 + w, -14)]
    elif kind == "both": o += [RF(x0, -14, w / 2.0, 28), L(x0 + w / 2.0, -14, x0 + w, 14), L(x0 + w / 2.0, 14, x0 + w, -14)]
    elif kind == "diag": o += [L(x0, -14, x0 + w, 14)]
    elif kind == "tri": o += [P(x0, -14, x0 + w, 0, x0, 14)]
    return o

G("--- M Bobine")
add("mb_a", "Bobine A1-A2", "K", [(0, -40), (0, 40)], coil(), props(bornes("A1-A2")), "coil")
add("mb_b", "Bobine B1-B2", "K", [(0, -40), (0, 40)], coil(), props(bornes("B1-B2")), "coil")
G("--- M Contacteur")
add("mc_km", "Bobine de contacteur KM", "KM", [(0, -40), (0, 40)], coil(), props(bornes("A1-A2")), "coil")
G("--- M Relais")
add("mr_ka", "Bobine de relais KA", "KA", [(0, -40), (0, 40)], coil(), props(bornes("A1-A2")), "coil")

# ------------------------------------------------------------------ sectionneurs / fusibles
def sec_pole(x, fuse=False, bar=True, circle=False, num=None):
    o = [L(x, -40, x, -20)]
    if bar: o += [L(x - 6, -20, x + 6, -20)]
    if circle: o += [C(x, -17, 2.5)]
    o += [L(x, 26, x - 16, -16), L(x, 26, x, 40)]
    if fuse: o += [rotrect(x - 8, 5, 22, 7, -16, -42)]
    if num: o += [TL(x + 5, -32, 8, num[0]), TL(x + 5, 32, 8, num[1])]
    return o
def handle(x):                               # poignee de commande manuelle
    return [L(x - 4, -34, x + 8, -4), L(x - 8, -31, x, -37), C(x + 10, 0, 4)]
def sec_multi(labels, fuse=True, hand=False, bar=True, circle=False, nfirst=False):
    k = len(labels); xs = xs_of(k); o = []; pins = []
    for i, x in enumerate(xs):
        o += sec_pole(x, fuse and not (nfirst and i == 0), bar, circle, labels[i])
        pins += [(x, -40), (x, 40)]
    x0 = xs[0] - 9
    if k > 1 or hand:
        o += [D((xs[0] - 48 if hand else x0), 5, xs[-1] - 9, 5)]
    if hand: o += handle(xs[0] - 58)
    return pins, o
NUM = [("1", "2"), ("3", "4"), ("5", "6"), ("7", "8")]
G("--- M Sectionneur")
p, o = sec_multi(NUM[:3]); add("ms_3", "Sectionneur porte-fusibles 3P", "Q", p, o)
p, o = sec_multi([("N", "N")] + NUM[:3], nfirst=True); add("ms_3n", "Sectionneur porte-fusibles 3P + N", "Q", p, o)

# ------------------------------------------------------------------ X Contact
G("--- X Contact")
NCS = ["01-02", "1-2", "11-12", "21-22", "31-32", "41-42", "51-52", "61-62", "71-72", "81-82", "91-92"]
NOS = ["03-04", "3-4", "13-14", "23-24", "33-34", "43-44", "53-54", "63-64", "73-74", "83-84", "93-94"]
COS = ["11-12-14", "15-16-18", "21-22-24", "31-32-34", "41-42-44"]
def contact_set(tag, pref, with_blank=True, cos=COS):
    for b in NCS:
        add("%s_nc_%s" % (tag, b.replace("-", "_")), "Contact NC %s" % b, pref, [(0, -40), (0, 40)], NC() + lab(), props(bob(), bornes(b)), "contact")
    if with_blank:
        add("%s_nc" % tag, "Contact NC", pref, [(0, -40), (0, 40)], NC(), props(bob()), "contact")
    for b in NOS:
        add("%s_no_%s" % (tag, b.replace("-", "_")), "Contact NO %s" % b, pref, [(0, -40), (0, 40)], NO() + lab(), props(bob(), bornes(b)), "contact")
    if with_blank:
        add("%s_no" % tag, "Contact NO", pref, [(0, -40), (0, 40)], NO(), props(bob()), "contact")
    for b in cos:
        add("%s_co_%s" % (tag, b.replace("-", "_")), "Contact inverseur %s" % b, pref, co_pins(), CO(), props(bob(), bornes(b)), "contact")
contact_set("xc", "K")
def tempo_set(tag):
    add(tag + "_nc_t", "Contact NC temporisé au travail 65-66", "KT", [(0, -40), (0, 40)], NC() + lab() + tempo(True, "t"), props(bob("KT1"), bornes("65-66")), "contact")
    add(tag + "_no_t", "Contact NO temporisé au travail 57-58", "KT", [(0, -40), (0, 40)], NO() + lab() + tempo(False, "t"), props(bob("KT1"), bornes("57-58")), "contact")
    add(tag + "_co_t", "Contact inverseur temporisé 15-16-18", "KT", co_pins(), CO() + tempo(False, "b"), props(bob("KT1"), bornes("15-16-18")), "contact")
    add(tag + "_nc_r", "Contact NC temporisé au repos 55-56", "KT", [(0, -40), (0, 40)], NC() + lab() + tempo(True, "r"), props(bob("KT1"), bornes("55-56")), "contact")
    add(tag + "_no_r", "Contact NO temporisé au repos 67-68", "KT", [(0, -40), (0, 40)], NO() + lab() + tempo(False, "r"), props(bob("KT1"), bornes("67-68")), "contact")
def thermal_set(tag):
    add(tag + "_nc", "Contact thermique NC 95-96", "F", [(0, -40), (0, 40)], NC() + lab() + thermal_mark(True), props(bob("F1"), bornes("95-96")), "thcontact")
    add(tag + "_no", "Contact thermique NO 97-98", "F", [(0, -40), (0, 40)], NO() + lab() + thermal_mark(False), props(bob("F1"), bornes("97-98")), "thcontact")

# ------------------------------------------------------------------ contacteurs (poles de puissance)
def kpole(x, a=None, b=None, arc=True, px=8):
    o = [L(x, -40, x, -16), L(x, 26, x - 16, -16), L(x, 26, x, 40)]
    if arc: o += [A(x - 5, -26, 10, 10, 90, 180)]
    if a: o += [TL(x + 5, -31, px, a), TL(x + 5, 32, px, b)]
    return o
PW = [("1/L1", "2/T1"), ("3/L2", "4/T2"), ("5/L3", "6/T3"), ("7/L4", "8/T4")]
def contactor(k, labels=PW, arc=True, strong=False):
    xs = xs_of(k); o = []; pins = []
    for i, x in enumerate(xs):
        o += kpole(x, labels[i][0] if labels else None, labels[i][1] if labels else None, arc, 7 if labels is PW else 8)
        if strong: o += [W(4), L(x - 11, 8, x - 8, 0), W(2)]
        pins += [(x, -40), (x, 40)]
    if k > 1: o += [D(xs[0] - 8, 4, xs[-1] - 9, 4)]
    return pins, o
G("--- X Contact contacteur")
xs = xs_of(4)
o = NO(xs[0]) + lab(xs[0], "1", "2") + NC(xs[1]) + lab(xs[1], "R1", "R2") + NC(xs[2]) + lab(xs[2], "R3", "R4") + NO(xs[3]) + lab(xs[3], "3", "4")
o += [A(x - 5, -26, 10, 10, 90, 180) for x in xs] + [D(xs[0] - 8, 4, xs[3] - 9, 4)]
add("xk_4m", "Contacteur 4P (2 NO + 2 NC)", "KM", sum([[(x, -40), (x, 40)] for x in xs], []), o, props(bob()), "power")
for k, nm in [(2, "2P"), (4, "4P"), (3, "3P"), (1, "1P")]:
    p, o = contactor(k); add("xk_%d" % k, "Contacteur %s" % nm, "KM", p, o, props(bob()), "power")
p, o = contactor(3, strong=True); add("xk_3r", "Contacteur 3P (contacts renforcés)", "KM", p, o, props(bob()), "power")

G("--- X Contact temporisé"); tempo_set("xt")
G("--- X Contact thermique"); thermal_set("xh")

# ------------------------------------------------------------------ disjoncteurs (hauteur 120)
def brk_pole(x, cross=True, iso=False, therm=True, mag=True, num=None):
    o = [L(x, -60, x, -39)]
    if cross: o += [L(x - 3, -42, x + 3, -36), L(x - 3, -36, x + 3, -42)]
    if iso: o += [L(x - 5, -46, x + 5, -46)]
    o += [L(x, -1, x - 15, -39)]
    pts = [x, -1]
    if therm: pts += [x, 19, x + 8, 19, x + 8, 25, x, 25]
    pts += [x, 30 if mag else 60]
    o += [P(*pts)]
    if mag: o += [A(x - 6, 30, 12, 12, 90, -180), L(x, 42, x, 60)]
    if num: o += [TL(x + 5, -53, 8, num[0]), TL(x + 5, 53, 8, num[1])]
    return o
def breaker(k, style="S", iso=False, neutral=False, hand=False, therm=True, mag=True):
    """S : liaison simple ; C : cadre pointille thermique + magnetique ; A : differentiel complet ; B : differentiel (tore seul)"""
    xs = xs_of(k); o = []; pins = []
    for i, x in enumerate(xs):
        isn = neutral and i == 0
        num = ("N", "N") if isn else NUM[i - (1 if neutral else 0)]
        o += brk_pole(x, cross=not isn, iso=iso and not isn, therm=therm and not isn, mag=mag and not isn, num=num)
        pins += [(x, -60), (x, 60)]
    x1, xn = xs[0], xs[-1]; xl = x1 - 22
    if style == "S":
        if k > 1: o += [D(x1 - 8, -20, xn - 8, -20)]
    else:
        o += [D(xl, -18, xn - 8, -18)]
        if style in ("A", "B"):
            o += [RR(x1 - 10, 4, (xn - x1) + 20, 10), D(xl, 9, x1 - 10, 9)]
        if style == "B": o += [D(xl, -18, xl, 9)]
        else:
            o += [D(xl, -18, xl, 36)]
            if therm: o += [D(xl, 22, xn + 8, 22)]
            if mag: o += [D(xl, 36, xn + 6, 36)]
    if hand:
        hx = xl - 26
        o += [D(hx + 14, -18, xl, -18), R(xl - 9, -22, 8, 8), L(hx - 4, -46, hx + 10, -14), L(hx - 9, -43, hx + 1, -49), C(hx + 12, -9, 4)]
    return pins, o
def gv2(dbl=False):
    xs = xs_of(3); o = []; pins = []
    for i, x in enumerate(xs):
        o += [L(x, -80, x, -61), L(x - 3, -64, x + 3, -58), L(x - 3, -58, x + 3, -64), L(x - 5, -67, x + 5, -67),
              L(x, -21, x - 14, -57), L(x, -21, x, -12),
              R(x - 17, -12, 34, 35), P(x, -12, x, -4, x + 11, -4, x + 11, 15, x, 15, x, 23),
              R(x - 17, 23, 34, 33), TI(x, 40, 17, "I>>" if dbl else "I>"), L(x, 56, x, 80),
              TL(x + 6, -73, 7, PW[i][0]), TL(x + 6, 72, 7, PW[i][1])]
        pins += [(x, -80), (x, 80)]
    x1 = xs[0]
    o += [R(x1 - 52, -53, 28, 28), L(x1 - 38, -53, x1 - 38, -25), L(x1 - 52, -39, x1 - 24, -39),
          L(x1 - 62, -39, x1 - 52, -39), L(x1 - 62, -45, x1 - 62, -33),
          D(x1 - 24, -39, xs[2] - 7, -39), D(x1 - 38, -25, x1 - 38, 40), D(x1 - 38, 2, x1 - 17, 2), D(x1 - 38, 40, x1 - 17, 40)]
    return pins, o

G("--- X Disjoncteur")
def brk_family(tag, items):
    for sid, nm, kw in items:
        p, o = breaker(**kw); add("%s_%s" % (tag, sid), nm, "Q", p, o, props(prop("Calibre", "16", "A", "Courant assigné", True)))
FAM = [
    ("b2", "Disjoncteur différentiel 2P", dict(k=2, style="B")),
    ("b4", "Disjoncteur différentiel 4P", dict(k=4, style="B")),
    ("b3", "Disjoncteur différentiel 3P", dict(k=3, style="B")),
    ("a2", "Disjoncteur différentiel 2P (déclencheurs liés)", dict(k=2, style="A")),
    ("a2s", "Disjoncteur différentiel 2P sectionnable", dict(k=2, style="A", iso=True)),
    ("a4", "Disjoncteur différentiel 4P (déclencheurs liés)", dict(k=4, style="A")),
    ("a3", "Disjoncteur différentiel 3P (déclencheurs liés)", dict(k=3, style="A")),
    ("an", "Disjoncteur différentiel Ph + N", dict(k=2, style="A", neutral=True)),
    ("a4s", "Disjoncteur différentiel 4P sectionnable", dict(k=4, style="A", iso=True)),
    ("a3s", "Disjoncteur différentiel 3P sectionnable", dict(k=3, style="A", iso=True)),
    ("s2", "Disjoncteur magnéto-thermique 2P", dict(k=2, style="S")),
    ("c2", "Disjoncteur magnéto-thermique 2P (déclencheurs liés)", dict(k=2, style="C")),
    ("c2s", "Disjoncteur magnéto-thermique 2P sectionnable", dict(k=2, style="C", iso=True)),
    ("cn", "Disjoncteur magnéto-thermique Ph + N", dict(k=2, style="C", neutral=True)),
    ("c4", "Disjoncteur magnéto-thermique 4P (déclencheurs liés)", dict(k=4, style="C")),
    ("c3", "Disjoncteur magnéto-thermique 3P (déclencheurs liés)", dict(k=3, style="C")),
    ("c1", "Disjoncteur magnéto-thermique 1P (déclencheurs liés)", dict(k=1, style="C", iso=True)),
    ("s4", "Disjoncteur magnéto-thermique 4P", dict(k=4, style="S")),
    ("s3", "Disjoncteur magnéto-thermique 3P", dict(k=3, style="S")),
    ("s1", "Disjoncteur magnéto-thermique 1P", dict(k=1, style="S")),
]
brk_family("xd", FAM)
p, o = gv2(); add("xd_gv", "Disjoncteur moteur magnéto-thermique 3P", "Q", p, o, props(prop("Réglage", "4 à 6,3", "A", "Plage de réglage thermique", True)))
p, o = breaker(2, "S", therm=False); add("xd_m2", "Disjoncteur magnétique 2P", "Q", p, o)

# ------------------------------------------------------------------ actionneur et verrouillage
G("Actionneur et verrouillage")
add("av_1", "Verrouillage mécanique (triangle bas)", "X", [], [D(-30, 0, -10, 0), D(10, 0, 30, 0), Q(-10, -9, 10, -9, 0, 9)])
add("av_2", "Verrouillage mécanique (triangle droite)", "X", [], [D(0, -30, 0, -10), D(0, 10, 0, 30), Q(-9, -10, -9, 10, 9, 0)])
add("av_3", "Verrouillage mécanique (triangle gauche)", "X", [], [D(0, -30, 0, -10), D(0, 10, 0, 30), Q(9, -10, 9, 10, -9, 0)])
add("av_4", "Verrouillage mécanique (triangle haut)", "X", [], [D(-30, 0, -10, 0), D(10, 0, 30, 0), Q(-10, 9, 10, 9, 0, -9)])

# ------------------------------------------------------------------ alimentations
G("Alimentations")
add("al_1", "Alimentation AC/DC (N L / 0V 24V)", "G", [(-20, -60), (20, -60), (-20, 60), (20, 60)],
    [R(-40, -40, 80, 80), L(-40, 40, 40, -40), L(-20, -60, -20, -40), L(20, -60, 20, -40), L(-20, 40, -20, 60), L(20, 40, 20, 60),
     T(-18, -16, 16, "~"), T(18, 18, 16, "="), TL(-16, -50, 8, "N"), TL(24, -50, 8, "L"), TL(-16, 50, 8, "0V"), TL(24, 50, 8, "24V")])
add("al_2", "Alimentation AC/DC avec terre", "G", [(-20, -60), (20, -60), (-60, 0), (-40, 60), (-20, 60), (20, 60), (40, 60)],
    [R(-50, -40, 100, 80), L(-50, 40, 50, -40), L(-20, -60, -20, -40), L(20, -60, 20, -40), L(-60, 0, -50, 0),
     L(-40, 40, -40, 60), L(-20, 40, -20, 60), L(20, 40, 20, 60), L(40, 40, 40, 60),
     T(-24, -16, 16, "~"), T(26, 18, 16, "="), TL(-16, -50, 8, "N"), TL(24, -50, 8, "L"), TR(-52, -8, 8, "GND"),
     T(-40, 33, 7, "0V"), T(-20, 33, 7, "0V"), T(20, 33, 7, "24V"), T(40, 33, 7, "24V")])
add("al_3", "Alimentation 400 Vac / 24 Vdc", "G", [(-20, -60), (20, -60), (-20, 60), (20, 60)],
    [R(-60, -40, 120, 80), L(-60, 40, 60, -40), L(-20, -60, -20, -40), L(20, -60, 20, -40), L(-20, 40, -20, 60), L(20, 40, 20, 60),
     T(-28, -16, 10, "400Vac"), T(28, 18, 10, "24Vdc")])
add("al_4", "Alimentation redressée 0-230-400 V", "G", [(-20, -60), (0, -60), (20, -60), (-10, 60), (10, 60), (-40, 60)],
    [R(-30, -40, 60, 80), L(-20, -60, -20, -40), L(0, -60, 0, -40), L(20, -60, 20, -40), L(-10, 40, -10, 60), L(10, 40, 10, 60),
     T(-20, -33, 7, "0V"), T(0, -33, 7, "230V"), T(20, -33, 7, "400V"), T(-10, 33, 7, "-"), T(10, 33, 7, "+"),
     A(-20, -6, 10, 10, 0, 180), A(-10, -6, 10, 10, 180, 180), L(2, -12, -4, 12), L(6, -3, 22, -3), D(6, 4, 22, 4),
     L(-40, 60, -40, 44), L(-48, 44, -32, 44), L(-45, 48, -35, 48), L(-42, 52, -38, 52)])

# ------------------------------------------------------------------ arrivees reseau
G("Arrivées Réseau Triphasé")
def arrivee(id, name, labels, dashed=True, vertical=False):
    k = len(labels); o = []; pins = []
    if vertical:
        ys = xs_of(k, 20)
        for y, l in zip(ys, labels):
            pins.append((0, y)); o += [TR(-6, y, 8, l)]
        o += [L(0, ys[0] - 14, 0, ys[-1] + 14), L(0, ys[0] - 14, -8, ys[0] - 14), L(0, ys[-1] + 14, -8, ys[-1] + 14)]
    else:
        xs = xs_of(k, 40); w = xs[-1] - xs[0] + 40; x0 = xs[0] - 20
        for x, l in zip(xs, labels):
            pins.append((x, -20)); o += [T(x, -8, 9, l)]
        if dashed: o += [DP(x0, -20, x0 + w, -20, x0 + w, 30, x0, 30, x0, -20)]
        else: o += [R(x0, -20, w, 50)]
    add(id, name, "X", pins, o)
arrivee("ar_2", "Arrivée réseau 2P (L1 L2 PE)", ["L1", "L2", "PE"])
arrivee("ar_3", "Arrivée réseau 3P (L1 L2 L3 PE)", ["L1", "L2", "L3", "PE"])
arrivee("ar_3n", "Arrivée réseau 3P + N (L1 L2 L3 N PE)", ["L1", "L2", "L3", "N", "PE"])
arrivee("ar_2c", "Arrivée réseau 2P - cadre plein", ["L1", "L2", "PE"], dashed=False)
arrivee("ar_3nv", "Arrivée réseau 3P + N verticale", ["L1", "L2", "L3", "N", "PE"], vertical=True)
arrivee("ar_3c", "Arrivée réseau 3P - cadre plein", ["L1", "L2", "L3", "PE"], dashed=False)

# ------------------------------------------------------------------ bobines (dossier general)
G("Bobines")
add("bo_g", "Bobine (grand modèle)", "K", [(0, -60), (0, 60)], [L(0, -60, 0, -25), L(0, 25, 0, 60), R(-40, -25, 80, 50)], "", "coil")
add("bo_2v", "Double bobine à verrouillage mécanique", "K", [(-40, -40), (-40, 40), (40, -40), (40, 40)],
    [L(-40, -40, -40, -14), L(-40, 14, -40, 40), R(-65, -14, 50, 28), L(40, -40, 40, -14), L(40, 14, 40, 40), R(15, -14, 50, 28),
     DP(-30, 14, -24, 26, -8, 26), DP(30, 14, 24, 26, 8, 26), Q(-8, 20, 8, 20, 0, 32)], "", "coil")
add("bo_acc0", "Bobine à accrochage avec remise à zéro", "K", [(0, -40), (0, 40)],
    [L(0, -40, 0, -22), L(0, 22, 0, 40), R(-60, -22, 80, 44), R(20, -22, 40, 44), C(40, 0, 13), L(28, -38, 40, -38), P(36, -42, 40, -38, 36, -34),
     T(47, -38, 9, "0"), L(36, -28, 44, -28), L(38, -28, 38, -22), L(42, -28, 42, -22)], "", "coil")
add("bo_acc", "Bobine à accrochage", "K", [(0, -40), (0, 40)],
    [L(0, -40, 0, -22), L(0, 22, 0, 40), R(-60, -22, 80, 44), R(20, -22, 40, 44), C(40, 0, 13)], "", "coil")
add("bo_2a", "Double bobine couplée", "K", [(-40, -40), (-40, 40), (40, -40), (40, 40)],
    [L(-40, -40, -40, -14), L(-40, 14, -40, 40), R(-65, -14, 50, 28), L(40, -40, 40, -14), L(40, 14, 40, 40), R(15, -14, 50, 28), D(-15, 0, 15, 0), T(0, -7, 8, "~")], "", "coil")
add("bo_spi", "Bobine de relais à impulsion", "K", [(20, -40), (20, 40)],
    [L(20, -40, 20, -22), L(20, 22, 20, 40), R(-40, -22, 120, 44), R(-40, -22, 44, 44), R(-34, -16, 32, 32), R(-28, -10, 20, 20), R(-22, -4, 8, 8)], "", "coil")
add("bo_pol", "Bobine de relais polarisé", "K", [(20, -40), (20, 40)],
    [L(20, -40, 20, -22), L(20, 22, 20, 40), R(-40, -22, 120, 44), R(-40, -22, 44, 44), L(-40, -22, 4, 22), L(-40, 22, 4, -22)], "", "coil")
add("bo_a", "Bobine A1-A2", "K", [(0, -40), (0, 40)], coil(), props(bornes("A1-A2")), "coil")
add("bo_b", "Bobine B1-B2", "K", [(0, -40), (0, 40)], coil(), props(bornes("B1-B2")), "coil")
add("bo_diag", "Bobine à case diagonale", "K", [(0, -40), (0, 40)], coil_side("diag"), props(bornes("A1-A2")), "coil")
add("bo_tri", "Bobine à case triangle", "K", [(0, -40), (0, 40)], coil_side("tri"), props(bornes("A1-A2")), "coil")
add("bo_tr", "Bobine temporisée au repos", "KT", [(0, -40), (0, 40)], coil_side("black"), props(bornes("A1-A2")), "coil")
add("bo_trt", "Bobine temporisée travail et repos", "KT", [(0, -40), (0, 40)], coil_side("both"), props(bornes("A1-A2")), "coil")
add("bo_tt", "Bobine temporisée au travail", "KT", [(0, -40), (0, 40)], coil_side("cross"), props(bornes("A1-A2")), "coil")
def self_core(solid):
    o = [L(0, -40, 0, -20)] + [A(-6, -20 + 10 * i, 12, 10, 90, -180) for i in range(4)] + [L(0, 20, 0, 40), TL(6, -32, 8, "1"), TL(6, 33, 8, "2")]
    if solid: o += [L(12, -20, 12, 20), L(16, -20, 16, 20)]
    else: o += [L(12, -20, 12, -2), L(16, -20, 16, -2), L(12, 2, 12, 20), L(16, 2, 16, 20)]
    return o
add("bo_self1", "Inductance à noyau fractionné", "L", [(0, -40), (0, 40)], self_core(False))
add("bo_self2", "Inductance à noyau de fer", "L", [(0, -40), (0, 40)], self_core(True))

# ------------------------------------------------------------------ bornes
def bornes_fam(tag, title, deco):
    arr = [("b", "broche en bas", [(0, 20)]), ("d", "broche à droite", [(20, 0)]), ("g", "broche à gauche", [(-20, 0)]),
           ("h", "broche en haut", [(0, -20)]), ("hdb", "3 broches (haut, droite, bas)", [(0, -20), (20, 0), (0, 20)]),
           ("ghb", "3 broches (gauche, haut, bas)", [(-20, 0), (0, -20), (0, 20)]),
           ("4", "4 broches", [(0, -20), (20, 0), (0, 20), (-20, 0)]), ("hb", "traversante verticale", [(0, -20), (0, 20)]),
           ("gd", "traversante horizontale", [(-20, 0), (20, 0)])]
    for sid, nm, pins in arr:
        o = [L(px * 0.45, py * 0.45, px, py) for px, py in pins] + deco()
        add("%s_%s" % (tag, sid), "%s - %s" % (title, nm), "X", pins, o, props(prop("Numéro", "1", "", "Numéro de la borne", True)))
G("Bornes - Divers")
add("bd_x", "Borne à voyant", "X", [(0, -20), (0, 20)], [L(0, -20, 0, -9), L(0, 9, 0, 20), C(0, 0, 9), L(-6.4, -6.4, 6.4, 6.4), L(-6.4, 6.4, 6.4, -6.4)])
add("bd_c1", "Borne ronde - broche en bas", "X", [(0, 20)], [C(0, 0, 8), L(0, 8, 0, 20)])
add("bd_c2", "Borne ronde - broche à droite", "X", [(20, 0)], [W(3), C(0, 0, 10), W(2), L(10, 0, 20, 0)])
add("bd_c3", "Borne ronde - broche à gauche", "X", [(-20, 0)], [W(3), C(0, 0, 10), W(2), L(-10, 0, -20, 0)])
add("bd_c4", "Borne ronde - broche en haut", "X", [(0, -20)], [W(3), C(0, 0, 10), W(2), L(0, -10, 0, -20)])
add("bd_fu", "Borne à fusible", "X", [(0, -40), (0, 40)], [L(0, -40, 0, -24), L(-6, -30, 6, -30), R(-5, -24, 10, 48), L(0, -24, 0, 24), L(0, 24, 0, 40), L(-6, 30, 6, 30)])
add("bd_se", "Borne sectionnable", "X", [(0, -40), (0, 40)], [L(0, -40, 0, -24), L(-6, -24, 6, -24), W(3), L(0, -18, 0, 18), W(2), L(-6, 24, 6, 24), L(0, 24, 0, 40)])
add("bd_s1", "Borne carrée - broche en haut", "X", [(0, -20)], [W(3), R(-10, -6, 20, 20), W(2), L(0, -6, 0, -20)])
add("bd_s3", "Borne carrée - 3 broches", "X", [(0, -20), (20, 0), (0, 20)], [W(3), R(-8, -8, 16, 16), W(2), L(0, -8, 0, -20), L(8, 0, 20, 0), L(0, 8, 0, 20)])
add("bd_s3g", "Borne carrée - 3 broches (gauche)", "X", [(-20, 0), (0, -20), (0, 20)], [W(3), R(-8, -8, 16, 16), W(2), L(0, -8, 0, -20), L(-8, 0, -20, 0), L(0, 8, 0, 20)])
add("bd_s4", "Borne carrée - 4 broches", "X", [(0, -20), (20, 0), (0, 20), (-20, 0)], [W(3), R(-8, -8, 16, 16), W(2), L(0, -8, 0, -20), L(8, 0, 20, 0), L(0, 8, 0, 20), L(-8, 0, -20, 0)])
add("bd_s2", "Borne carrée - traversante", "X", [(0, -20), (0, 20)], [W(3), R(-8, -8, 16, 16), W(2), L(0, -8, 0, -20), L(0, 8, 0, 20)])
add("bd_pe", "Borne de terre", "X", [(0, -20), (0, 20)], [L(0, -20, 0, 20), C(0, 0, 5), O("#00aa00"), W(3), L(-8, -8, 8, 8), W(2), O()])
add("bd_sb", "Borne carrée - broche en bas", "X", [(0, 20)], [W(3), R(-12, -16, 24, 24), W(2), L(0, 8, 0, 20)])
add("bd_cou", "Borne à couteau sectionnable", "X", [(0, -40), (0, 40)], [L(0, -40, 0, -20), L(-5, -20, 5, -20), L(0, 26, -14, -14), W(4), L(-4, 14, -9, 0), W(2), L(0, 26, 0, 40), TL(5, -32, 8, "1"), TL(5, 33, 8, "2")])
G("Bornes - Point")
bornes_fam("bp", "Borne point", lambda: [C(0, 0, 9), F(0, 0, 5)])
G("Bornes - Standard")
bornes_fam("bs", "Borne standard", lambda: [C(0, 0, 9), L(-13, -13, 13, 13)])
G("Borniers")
add("bn_1", "Bornier (1 borne, vue de face)", "X", [(0, -60), (0, 60)], [R(-16, -50, 32, 100), RF(-16, -12, 32, 24), C(0, -32, 6), C(0, 32, 6), L(0, -60, 0, -50), L(0, 50, 0, 60)])
def bornier(k):
    xs = xs_of(k, 20); o = []; pins = []
    x0 = xs[0] - 10; w = 20 * k
    o += [R(x0, -30, w, 60), L(x0, -6, x0 + w, -6), L(x0, 6, x0 + w, 6)]
    for i, x in enumerate(xs):
        o += [C(x, -18, 5), L(x - 5, -18, x + 5, -18), L(x, -23, x, -13), C(x, 18, 5), L(x - 5, 18, x + 5, 18), L(x, 13, x, 23),
              L(x, -40, x, -30), L(x, 30, x, 40), T(x, 0, 7, str(i + 1))]
        if i: o += [L(x - 10, -30, x - 10, 30)]
        pins += [(x, -40), (x, 40)]
    return pins, o
for k in (3, 4, 10):
    p, o = bornier(k); add("bn_%d" % k, "Bornier %d bornes" % k, "X", p, o)

# ------------------------------------------------------------------ BP, commutateurs, voyants
def pb(id, name, nc, act, b, pref="S"):
    add(id, name, pref, [(0, -40), (0, 40)], CT(nc) + lab() + actuator(act, nc), props(bornes(b)))
def lamp(id, name, color=None, tag=True, hook=False, big=False):
    r = 14 if big else 11
    o = [L(0, -40, 0, -r), L(0, r, 0, 40)]
    o += [E(0, 0, r, color)] if color else [C(0, 0, r)]
    d = r * 0.707
    o += [L(-d, -d, d, d), L(-d, d, d, -d)]
    if tag: o += [TL(5, -30, 8, "X1"), TL(5, 32, 8, "X2")]
    if hook: o += [D(r + 2, 0, r + 10, 0), P(r + 10, -5, r + 15, -5, r + 15, 5, r + 10, 5)]
    add(id, name, "H", [(0, -40), (0, 40)], o)
def hswitch(id, name, nc):                       # interrupteur de position represente a l'horizontale
    o = [L(-40, 0, -25, 0), C(-20, 0, 5), C(20, 0, 5), L(25, 0, 40, 0)]
    if nc: o += [L(-15, -2, 30, 5), P(-4, -4, 8, -4, 8, 1)]
    else: o += [L(-15, -2, 28, -18), P(2, -8, 12, -12, 14, -6)]
    add(id, name, "S", [(-40, 0), (40, 0)], o)
G("BP & Commut & Voyants")
pb("bv_au_nc", "Arrêt d'urgence NC 1-2", True, "mush", "1-2")
pb("bv_auk_nc", "Arrêt d'urgence à clé NC 1-2", True, "mushkey", "1-2")
pb("bv_au_no", "Arrêt d'urgence NO 3-4", False, "mush", "3-4")
pb("bv_auk_no", "Arrêt d'urgence à clé NO 3-4", False, "mushkey", "3-4")
pb("bv_bp_no", "Bouton poussoir NO", False, "E", "3-4")
pb("bv_bp_nc", "Bouton poussoir NC", True, "E", "1-2")
pb("bv_bpl_nc", "Bouton poussoir lumineux NC 1-2", True, "lum", "1-2")
pb("bv_bpl_no", "Bouton poussoir lumineux NO 3-4", False, "lum", "3-4")
pb("bv_cle", "Commutateur à clé NO", False, "selkey", "3-4")
hswitch("bv_h_nc", "Interrupteur de position NC (horizontal)", True)
hswitch("bv_h_no", "Interrupteur de position NO (horizontal)", False)
lamp("bv_v_x", "Voyant X1-X2 (bloc lumineux)", hook=True)
lamp("bv_v", "Voyant X1-X2")
lamp("bv_v_w", "Voyant blanc", "#ffffff", tag=False, big=True)
lamp("bv_v_o", "Voyant orange", "#ff8a00", tag=False, big=True)
lamp("bv_v_r", "Voyant rouge", "#ff0000", tag=False, big=True)
lamp("bv_v_g", "Voyant vert", "#00e000", tag=False, big=True)

G("Compteurs")
add("cp_1", "Compteur (bornes 1 et 4)", "P", [(-40, -40), (40, -40)], [L(-40, -40, -40, -30), L(40, -40, 40, -30), R(-60, -30, 120, 80), R(-44, 0, 88, 24), TL(-44, -18, 10, "1"), TL(36, -18, 10, "4")])
add("cp_2", "Compteur d'impulsions (L N, RAZ)", "P", [(-10, -60), (10, -60), (-60, -20), (-60, 0), (-60, 20), (-60, 40), (60, -20), (60, 0)],
    [R(-40, -40, 80, 100), L(-10, -60, -10, -40), L(10, -60, 10, -40), TL(-14, -32, 8, "L"), TL(6, -32, 8, "N"),
     L(-60, -20, -40, -20), L(-60, 0, -40, 0), L(-60, 20, -40, 20), L(-60, 40, -40, 40), TL(-36, -20, 8, "0V"), TL(-36, 0, 8, "In A"), TL(-36, 20, 8, "+12V"), TL(-36, 40, 8, "RAZ"),
     L(60, -20, 40, -20), L(60, 0, 40, 0), DP(40, -20, 30, -20, 30, -14), DP(40, 0, 30, 0, 30, -4), L(30, -4, 26, -14)])

G("Condensateurs")
add("cd_pol", "Condensateur polarisé", "C", [(0, -40), (0, 40)],
    [L(0, -40, 0, -5), RF(-14, -5, 28, 5), P(-20, -8, -20, 8, 20, 8, 20, -8), L(0, 8, 0, 40), T(-10, -14, 12, "+"), TL(6, -30, 9, "1"), TL(6, 32, 9, "2")],
    props(prop("Capacité", "100", "µF", "Capacité", True)))
add("cd_np", "Condensateur", "C", [(0, -40), (0, 40)],
    [L(0, -40, 0, -5), L(-22, -5, 22, -5), L(-22, 5, 22, 5), L(0, 5, 0, 40), TL(6, -30, 9, "1"), TL(6, 32, 9, "2")],
    props(prop("Capacité", "10", "µF", "Capacité", True)))

G("Connecteurs")
def round4(id, name, male, labels):
    o = [C(0, 0, 24)]
    pts = [(-9, -9), (9, -9), (9, 9), (-9, 9)]
    for (x, y) in pts: o += [F(x, y, 4) if male else C(x, y, 4)]
    o += [TR(-27, -12, 8, labels[0]), TL(27, -12, 8, labels[1]), TL(27, 12, 8, labels[2]), TR(-27, 12, 8, labels[3])]
    if not male: o += [L(-3, 24, -3, 20), L(3, 24, 3, 20)]
    add(id, name, "X", [], o)
round4("cn_4f", "Fiche 4P femelle", False, ["BN", "WH", "BU", "BK"])
round4("cn_4m", "Fiche 4P mâle", True, ["WH", "BN", "BK", "BU"])
add("cn_m", "Contact mâle", "X", [(0, 40)], [W(6), L(0, -10, 0, 12), W(2), L(0, 12, 0, 40), TL(8, 26, 9, "1")])
add("cn_mf", "Contact mâle + femelle (vertical)", "X", [(0, -40), (0, 40)], [L(0, -40, 0, -16), W(5), L(0, -16, 0, 6), W(2), A(-8, 2, 16, 16, 180, 180), L(0, 18, 0, 40), TL(6, -32, 8, "2"), TL(6, 33, 8, "1")])
add("cn_mfh", "Contact mâle + femelle (horizontal)", "X", [(-40, 0), (40, 0)], [L(40, 0, 16, 0), W(5), L(16, 0, -6, 0), W(2), A(-18, -8, 16, 16, 90, 180), L(-18, 0, -40, 0), T(-30, -8, 8, "2"), T(30, -8, 8, "1")])
add("cn_f", "Contact femelle", "X", [(0, 40)], [A(-14, -18, 28, 28, 180, 180), L(0, 10, 0, 40), TL(8, 26, 9, "2")])
add("cn_fh", "Contact femelle (horizontal)", "X", [(40, 0)], [A(-18, -14, 28, 28, 270, 180), L(10, 0, 40, 0), T(26, -9, 9, "2")])

# ------------------------------------------------------------------ contacteurs
G("Contacteurs")
add("ck_assoc", "Bloc d'association 3 conducteurs", "X", [(-40, -40), (-40, 40), (0, -40), (0, 40), (40, -40), (40, 40)], [L(-40, -40, -40, 40), L(0, -40, 0, 40), L(40, -40, 40, 40)])
add("ck_nc", "Contact NC de contacteur", "KM", [(0, -40), (0, 40)], NC(), props(bob()), "contact")
add("ck_no", "Contact NO de contacteur", "KM", [(0, -40), (0, 40)], NO(), props(bob()), "contact")
p, o = contactor(3, labels=None); add("ck_3s", "Contacteur 3P (sans repères)", "KM", p, o, props(bob()), "power")
xs = xs_of(6)
o = []
for i, x in enumerate(xs): o += kpole(x)
o += [D(xs[0] - 8, 4, -10, 4), D(10, 4, xs[5] - 9, 4), Q(-8, -4, 8, -4, 0, 10)]
add("ck_inv", "Inverseur 2 x 3P à verrouillage mécanique", "KM", sum([[(x, -40), (x, 40)] for x in xs], []), o, props(bob()), "power")
xs = xs_of(4)
o = NO(xs[0]) + lab(xs[0], "1", "2") + NC(xs[1]) + lab(xs[1], "R1", "R2") + NC(xs[2]) + lab(xs[2], "R3", "R4") + NO(xs[3]) + lab(xs[3], "3", "4")
o += [A(x - 5, -26, 10, 10, 90, 180) for x in xs] + [D(xs[0] - 8, 4, xs[3] - 9, 4)]
add("ck_4m", "Contacteur 4P (2 NO + 2 NC)", "KM", sum([[(x, -40), (x, 40)] for x in xs], []), o, props(bob()), "power")
for k, nm in [(2, "2P"), (4, "4P"), (3, "3P"), (1, "1P")]:
    p, o = contactor(k); add("ck_%d" % k, "Contacteur %s" % nm, "KM", p, o, props(bob()), "power")
p, o = contactor(3, strong=True); add("ck_3r", "Contacteur 3P (contacts renforcés)", "KM", p, o, props(bob()), "power")
add("ck_bob", "Bobine de contacteur avec commande Auto / 0 / 1", "KM", [(0, -40), (0, 40)],
    coil(60, 30, "A1", "A2") + [DP(-70, -22, -56, -22, -56, 12, -70, 12), D(-56, -5, 56, -5) if False else DP(-56, -8, 60, -8, 60, 14, 80, 14), TR(-72, -22, 7, "1"), TR(-72, -5, 7, "Auto"), TR(-72, 12, 7, "0")], "", "coil")
xs = xs_of(2)
o = NO(xs[0]) + lab(xs[0], "1", "2") + NO(xs[1]) + lab(xs[1], "3", "4") + [D(-56, 4, -44, 4), P(-44, 4, -40, 12, -36, 4), D(-36, 4, xs[1] - 9, 4)]
add("ck_int2", "Interrupteur 2P à positions maintenues", "Q", [(xs[0], -40), (xs[0], 40), (xs[1], -40), (xs[1], 40)], o)

G("Contacts Auxiliaires")
add("ca_nc", "Contact auxiliaire NC", "K", [(0, -40), (0, 40)], NC(), props(bob()), "contact")
add("ca_nc_t", "Contact auxiliaire NC temporisé au travail", "KT", [(0, -40), (0, 40)], NC() + tempo(True, "t"), props(bob("KT1")), "contact")
add("ca_nc_r", "Contact auxiliaire NC temporisé au repos", "KT", [(0, -40), (0, 40)], NC() + tempo(True, "r"), props(bob("KT1")), "contact")
add("ca_no", "Contact auxiliaire NO", "K", [(0, -40), (0, 40)], NO(), props(bob()), "contact")
add("ca_no_t", "Contact auxiliaire NO temporisé au travail", "KT", [(0, -40), (0, 40)], NO() + tempo(False, "t"), props(bob("KT1")), "contact")
add("ca_no_r", "Contact auxiliaire NO temporisé au repos", "KT", [(0, -40), (0, 40)], NO() + tempo(False, "r"), props(bob("KT1")), "contact")

# ------------------------------------------------------------------ capteurs
def sensorbox(x, y, kind, w=36):
    """boitier de detecteur : losange barre + symbole de technologie"""
    o = [R(x - w / 2.0, y - 18, w, 36), Q(x - 10, y - 4, x, y - 14, x + 10, y - 4, x, y + 6), L(x, y - 14, x, y + 6)]
    if kind == "cap": o += [L(x - 6, y + 12, x - 2, y + 12), L(x - 2, y + 8, x - 2, y + 16), L(x + 2, y + 8, x + 2, y + 16), L(x + 2, y + 12, x + 6, y + 12)]
    elif kind == "ind": o += [A(x - 8, y + 9, 5.3, 6, 0, 180), A(x - 2.7, y + 9, 5.3, 6, 0, 180), A(x + 2.6, y + 9, 5.3, 6, 0, 180), L(x - 10, y + 12, x - 8, y + 12), L(x + 8, y + 12, x + 10, y + 12)]
    elif kind == "mag": o += [W(4), P(x - 12, y - 10, x - 15, y - 10, x - 15, y + 8, x - 12, y + 8), W(2)]
    return o
G("Contacts Capteurs")
for act, nm in [("cam", "Détecteur à came"), ("lev", "Détecteur de niveau"), ("oval", "Détecteur de vérin")]:
    pb("cc_%s_nc" % act, nm + " NC 21-22", True, act, "21-22", "B"); pb("cc_%s_no" % act, nm + " NO 13-14", False, act, "13-14", "B")
pb("cc_d_no", "Détecteur de proximité NO 13-14", False, "D", "13-14", "B")
for nc in (True, False):
    b = "BN-BU"
    add("cc_mag_%s" % ("nc" if nc else "no"), "Détecteur magnétique %s (BN / BU)" % ("NC" if nc else "NO"), "B", [(0, -40), (0, 40)],
        CT(nc) + sensorbox(-52, 0, "mag", 40) + [D(-32, 0, bx(nc) - 1, 0), TL(5, -30, 8, "BN"), TL(5, 32, 8, "BU")])
for act, nm in [("float", "Détecteur à flotteur"), ("P", "Pressostat")]:
    pb("cc_%s_nc" % act, nm + " NC 21-22", True, act, "21-22", "B"); pb("cc_%s_no" % act, nm + " NO 13-14", False, act, "13-14", "B")
for kind, nm in [("cap", "Détecteur capacitif"), ("ind", "Détecteur inductif")]:
    for nc in (True, False):
        add("cc_%s_%s" % (kind, "nc" if nc else "no"), "%s %s %s" % (nm, "NC" if nc else "NO", "1-2" if nc else "3-4"), "B", [(0, -40), (0, 40)],
            CT(nc) + sensorbox(-50, 0, kind) + [D(-32, 0, bx(nc) - 1, 0)] + lab(0, "1" if nc else "3", "2" if nc else "4"))
add("cc_th", "Thermistance", "R", [(0, -40), (0, 40)], [L(0, -40, 0, -22), R(-7, -22, 14, 44), L(0, 22, 0, 40), P(-14, -24, 14, 20, 14, 28), TL(6, -32, 8, "1"), TL(6, 33, 8, "2")])
add("cc_th3", "Sonde résistive 3 fils", "R", [(0, -40), (-20, 40), (0, 40)], [L(0, -40, 0, -22), R(-7, -22, 14, 44), L(0, 22, 0, 40), P(-14, -24, 14, 20, 14, 28), P(-20, 40, -20, 30, 0, 30), TL(8, -30, 10, "BC"), TR(-24, 26, 10, "R"), TL(4, 28, 10, "R")])
add("cc_tc", "Thermocouple", "B", [(-30, -40), (30, -40)], [L(-30, -40, -30, -4), L(30, -40, 30, 10), W(4), L(-30, -4, 0, 40), W(2), L(0, 40, 30, 10), TL(-24, -28, 12, "1"), TL(36, -28, 12, "2"), T(-18, 2, 14, "+"), T(42, 2, 14, "-")])
pb("cc_t_nc", "Thermostat NC 21-22", True, "temp", "21-22", "B"); pb("cc_t_no", "Thermostat NO 13-14", False, "temp", "13-14", "B")

# ------------------------------------------------------------------ fins de course
def tri_flag(nc):                             # triangle d'interrupteur de position porte par la lame
    if nc: return [P(bx(True, 0, -16), -16, -4, -16, bx(True, 0, 10), 10)] if False else [P(11, -16, -2, -16, 2, 18)]
    return [P(-11, -2, -18, 4, -5, 12)]
def snap(id, name, nc, wall):                 # rupteur a action brusque
    b = "21-22" if nc else "13-14"
    o = [L(0, -40, 0, -24), C(0, -19, 5), C(0, 19, 5), L(0, 24, 0, 40), W(5), (L(8, -24, 8, 24) if nc else L(-8, -24, -8, 24)), W(2)] + lab()
    if wall: o += [L(-26, -16, -26, 16)] + [L(-26, y, -34, y + 6) for y in (-14, -8, -2, 4, 10)] + [C(-20, 0, 2), L(-18, 0, -8 if not nc else -5, 0)]
    add(id, name, "S", [(0, -40), (0, 40)], o, props(bornes(b)))
G("Contacts Fin de course et rupteurs")
pb("fc_w_nc", "Fin de course NC 21-22 (butée)", True, "rollwall", "21-22")
pb("fc_w_no", "Fin de course NO 13-14 (butée)", False, "rollwall", "13-14")
pb("fc_nc", "Fin de course NC 21-22", True, "roll", "21-22")
pb("fc_no", "Fin de course NO 13-14", False, "roll", "13-14")
add("fc_t_nc", "Interrupteur de position NC 21-22", "S", [(0, -40), (0, 40)], NC() + lab() + tri_flag(True), props(bornes("21-22")))
add("fc_t_no", "Interrupteur de position NO 13-14", "S", [(0, -40), (0, 40)], NO() + lab() + tri_flag(False), props(bornes("13-14")))
hswitch("fc_h_no", "Interrupteur de position NO (horizontal)", False)
hswitch("fc_h_nc", "Interrupteur de position NC (horizontal)", True)
snap("fc_s_ncw", "Rupteur NC 21-22 (butée)", True, True); snap("fc_s_nc", "Rupteur NC 21-22", True, False)
snap("fc_s_now", "Rupteur NO 13-14 (butée)", False, True); snap("fc_s_no", "Rupteur NO 13-14", False, False)

G("Contacts Horloge")
add("ch_1", "Horloge L-N avec contact inverseur 2-4-6", "KH", [(-60, -40), (-60, 40), (0, 40), (-20, -40), (0, -40)],
    [L(-60, -40, -60, -14), C(-60, 0, 14), L(-60, 14, -60, 40), L(-60, 0, -60, -10), L(-60, 0, -52, 0), D(-46, 0, -6, 0), TL(-55, -32, 8, "L"), TL(-55, 33, 8, "N")] + CO(0, False) + [TL(-16, -30, 8, "2"), TL(4, -30, 8, "6"), TL(5, 32, 8, "4")])
add("ch_2", "Horloge 7-8 avec contact inverseur 1-2-3", "KH", [(-60, -40), (-60, 40), (0, 40), (-20, -40), (0, -40)],
    [L(-60, -40, -60, -14), C(-60, 0, 14), L(-60, 14, -60, 40), L(-60, 0, -60, -10), L(-60, 0, -52, 0), D(-46, 0, -6, 0), TL(-55, -32, 8, "7"), TL(-55, 33, 8, "8")] + CO(0, False) + [TL(-16, -30, 8, "1"), TL(4, -30, 8, "3"), TL(5, 32, 8, "2")])
add("ch_3", "Horloge 7-8 à deux contacts inverseurs", "KH", [(-60, -40), (-60, 40), (0, 40), (-20, -40), (0, -40), (60, 40), (40, -40), (60, -40)],
    [L(-60, -40, -60, -14), C(-60, 0, 14), L(-60, 14, -60, 40), L(-60, 0, -60, -10), L(-60, 0, -52, 0), D(-46, 0, 54, 0), TL(-55, -32, 8, "7"), TL(-55, 33, 8, "8")]
    + CO(0, False) + [TL(-16, -30, 8, "1"), TL(4, -30, 8, "3"), TL(5, 32, 8, "2")] + CO(60, False) + [TL(44, -30, 8, "4"), TL(64, -30, 8, "6"), TL(65, 32, 8, "5")])

G("Contacts NO NF")
contact_set("nn", "K", cos=COS + ["95-96-98"])
G("Contacts Relais Thermique"); thermal_set("rt")

G("Contacts Sélecteurs")
def sel(id, name, nc, b, key=False):
    add(id, name, "S", [(0, -40), (0, 40)], CT(nc) + lab() + actuator("selkey" if key else "sel", nc) + [T(-46, -12, 7, "1"), T(-32, -12, 7, "2")], props(bornes(b)))
sel("cs_no", "Sélecteur 2 positions NO 3-4", False, "3-4")
sel("cs_nc", "Sélecteur 2 positions NC 1-2", True, "1-2")
sel("cs_k", "Sélecteur à clé NO 3-4", False, "3-4", True)
o = NC(0) + lab(0, "1", "2") + NO(60) + lab(60, "3", "4") + actuator("sel", True, 0) + [D(8, 0, 49, 0), T(-46, -12, 7, "1"), T(-32, -12, 7, "2")]
add("cs_2", "Sélecteur 2 positions NC + NO", "S", [(0, -40), (0, 40), (60, -40), (60, 40)], o)
hswitch("cs_h_nc", "Contact de sélecteur NC (horizontal)", True)
hswitch("cs_h_no", "Contact de sélecteur NO (horizontal)", False)
o = NC(0) + lab(0, "21", "22") + NO(60) + lab(60, "13", "14") + [L(-16, 6, -10, 6), L(-13, 0, -13, 6) if False else P(-18, 8, -12, 8, -8, 0), D(-8, 0, 6, 0), D(8, 0, 49, 0)]
add("cs_ncno", "Contact double NC 21-22 + NO 13-14", "S", [(0, -40), (0, 40), (60, -40), (60, 40)], o)

G("Contacts Temporisés"); tempo_set("tp")

G("Contrôle")
add("co_1", "Contrôleur permanent d'isolement", "A", [(0, -40), (0, 40)], [L(0, -40, 0, -24), R(-16, -24, 32, 48), L(0, 24, 0, 40), TL(6, -32, 9, "1"), TL(6, 33, 9, "2")])
add("co_2", "Relais de contrôle de phases (L1 L2 L3, contact 15-16-18)", "KA", [(-40, -60), (0, -60), (40, -60), (-60, -10), (-40, 60), (-20, 60)],
    [R(-60, -50, 120, 100), R(-52, -50, 24, 16), R(-12, -50, 24, 16), R(28, -50, 24, 16), T(-40, -42, 8, "L1"), T(0, -42, 8, "L2"), T(40, -42, 8, "L3"),
     L(-40, -60, -40, -50), L(0, -60, 0, -50), L(40, -60, 40, -50), L(-60, -10, -20, -10), L(-20, -10, -20, 0), L(-20, 50, -20, 60), L(-40, 50, -40, 60),
     R(-48, 34, 16, 16), R(-28, 34, 16, 16), T(-40, 42, 7, "16"), T(-20, 42, 7, "18"), L(-20, 34, -26, 2), L(-40, 34, -40, 26), L(-40, 26, -30, 26), D(-22, -30, -22, 0) if False else DP(0, -34, 0, 12, -22, 12),
     C(30, 30, 5), T(30, 16, 7, "LED")])

# ------------------------------------------------------------------ detecteurs
G("Détecteurs de proximité")
def prox(id, name, kind, wires, contact=None):
    o = sensorbox(0, 0, kind, 40); pins = []
    if contact:                                # contact interne dessine dans le boitier
        o = [R(-26, -20, 52, 40), Q(-20, -4, -12, -12, -4, -4, -12, 4), L(-12, -12, -12, 4), W(4), P(-18, 12, -20, 12, -20, 6) if False else L(-20, 12, -4, 12), W(2),
             L(0, -20, 0, -8), L(0, 20, 0, 4), L(0, 4, 12, -10)]
        if contact == "co": o += [L(14, 4, 26, 4)]
    else:
        o = sensorbox(0, 0, kind, 40) if False else [R(-20, -20, 40, 40), Q(-10, -4, 0, -14, 10, -4, 0, 6), L(0, -14, 0, 6)] + sensorbox(0, 0, kind, 40)[3:]
    half = 26 if contact else 20
    for wname in wires:
        if wname == "BN": pins.append((0, -40)); o += [L(0, -40, 0, -20), TL(5, -32, 8, "BN")]
        if wname == "BU": pins.append((0, 40)); o += [L(0, 20, 0, 40), TL(5, 32, 8, "BU")]
        if wname == "BK": pins.append((40, 0)); o += [L(half, 0 if not contact else 4, 40, 0 if not contact else 4) if not contact else L(26, 0, 40, 0), T(32, -8, 8, "BK")]
        if wname == "WH": pins.append((-40, 0)); o += [L(-half, 0, -40, 0), T(-32, -8, 8, "WH")]
    add(id, name, "B", pins, o)
prox("dp_m2", "Détecteur magnétique 2 fils (contact NO)", "mag", ["BN", "BU"], "no")
prox("dp_m3", "Détecteur magnétique 3 fils (inverseur)", "mag", ["BN", "BU", "BK"], "co")
prox("dp_c2", "Détecteur capacitif 2 fils", "cap", ["BN", "BU"])
prox("dp_c3", "Détecteur capacitif 3 fils", "cap", ["BN", "BU", "BK"])
prox("dp_c4", "Détecteur capacitif 4 fils", "cap", ["BN", "BU", "BK", "WH"])
prox("dp_i2", "Détecteur inductif 2 fils", "ind", ["BN", "BU"])
prox("dp_i3", "Détecteur inductif 3 fils", "ind", ["BN", "BU", "BK"])

G("Détecteurs optiques")
def arrows_r(x, y): return [L(x - 7, y - 5, x + 3, y + 5), P(x - 1, y + 5, x + 3, y + 5, x + 3, y + 1), L(x + 1, y - 9, x + 11, y + 1), P(x + 7, y + 1, x + 11, y + 1, x + 11, y - 3)]
def reflector(x): return [R(x, -16, 8, 32)] + [L(x, y, x + 8, y - 6) for y in (-8, 0, 8, 16)]
add("do_r2", "Détecteur optique reflex 1-4", "B", [(0, -40), (0, 40)],
    [R(-20, -20, 40, 40), L(0, -40, 0, -20), L(0, 20, 0, 40)] + arrows_r(-2, 2) + reflector(40) + [P(32, -8, 24, -8), P(28, -11, 24, -8, 28, -5), P(32, 8, 24, 8), P(28, 5, 24, 8, 28, 11), TL(5, -30, 8, "1"), TL(5, 32, 8, "4")])
add("do_r3", "Détecteur optique reflex 1-3-4", "B", [(0, -40), (0, 40), (-40, 0)],
    [R(-20, -20, 40, 40), L(0, -40, 0, -20), L(0, 20, 0, 40), L(-40, 0, -20, 0)] + arrows_r(-2, 2) + reflector(40) + [P(32, -8, 24, -8), P(28, -11, 24, -8, 28, -5), P(32, 8, 24, 8), P(28, 5, 24, 8, 28, 11), TL(5, -30, 8, "1"), TL(5, 32, 8, "4"), T(-32, -8, 8, "3")])
add("do_e", "Émetteur optique (BN / BU)", "B", [(0, -40), (0, 40)],
    [R(-24, -16, 48, 32), L(0, -40, 0, -16), L(0, 16, 0, 40), K(-4, -7, -4, 7, 8, 0), L(8, -7, 8, 7), L(-12, 0, -4, 0), L(8, 0, 16, 0),
     L(32, -5, 46, -5), P(42, -8, 46, -5, 42, -2), L(32, 5, 46, 5), P(42, 2, 46, 5, 42, 8), TL(5, -30, 8, "BN"), TL(5, 32, 8, "BU")])
def optobox(id, name, out):
    o = [R(-20, -20, 40, 40), L(0, -40, 0, -20), L(0, 20, 0, 40), TL(5, -30, 8, "BN"),
         C(-10, -10, 3), L(-10, -15, -10, -17), L(-10, -5, -10, -3), L(-15, -10, -17, -10), L(-5, -10, -3, -10), L(-2, -10, 12, -10), P(8, -13, 12, -10, 8, -7), L(14, -16, 14, 14),
         Q(-14, 8, -4, 2, -4, 14), L(12, 8, -2, 8), P(2, 5, -2, 8, 2, 11)]
    pins = [(0, -40), (0, 40)]
    if out == "pnp": o += [L(20, 0, 40, 0), T(32, 8, 8, "PK"), TL(5, 32, 8, "BK")]; pins.append((40, 0))
    elif out == "2": o += [L(20, -10, 40, -10) if False else L(20, -20, 40, -20), L(20, 0, 40, 0), T(34, -27, 8, "BK"), T(34, -7, 8, "WH"), TL(5, 32, 8, "BU")]; pins += [(40, -20), (40, 0)]
    else: o += [TL(5, 32, 8, "BU")]
    add(id, name, "B", pins, o)
optobox("do_p", "Détecteur photoélectrique (BN BK, sortie PK)", "pnp")
optobox("do_4", "Détecteur photoélectrique 4 fils (BN BU BK WH)", "2")
optobox("do_2", "Détecteur photoélectrique 2 fils (BN BU)", "")
add("do_co", "Contact inverseur de détecteur (WH BK GY)", "B", [(0, 40), (-20, -40), (0, -40)], CO(0, False) + [TL(-16, -30, 8, "WH"), TL(4, -30, 8, "BK"), TL(5, 32, 8, "GY")])

# ------------------------------------------------------------------ disjoncteurs
G("Disjoncteurs")
brk_family("dj", FAM)
p, o = gv2(); add("dj_gv", "Disjoncteur moteur magnéto-thermique 3P", "Q", p, o, props(prop("Réglage", "4 à 6,3", "A", "Plage de réglage thermique", True)))

def c60_pole(x, therm=True, cross=True, num=None, arrow=False):
    o = [L(x, -60, x, -44)]
    if cross: o += [L(x - 3, -47, x + 3, -41), L(x - 3, -41, x + 3, -47)]
    o += [L(x, -10, x - 14, -44)]
    if therm: o += [P(x, -10, x, -4, x + 12, -4, x + 12, 10, x, 10, x, 14), A(x - 10, 14, 20, 20, 90, -180), L(x, 34, x, 60)]
    else: o += [L(x, -10, x, 10), A(x - 10, 10, 20, 20, 90, -180), L(x, 30, x, 60)]
    if num: o += [TL(x + 5, -53, 8, num[0]), TL(x + 5, 53, 8, num[1])]
    return o
def c60(k, therm=True, neutral=False):
    xs = xs_of(k); o = []; pins = []
    for i, x in enumerate(xs):
        if neutral and i == 0:
            o += [L(x, -60, x, -44), L(x, -10, x - 14, -44), L(x, -10, x, 60), TL(x + 5, -53, 8, "N"), TL(x + 5, 53, 8, "N")]
        else:
            o += c60_pole(x, therm, True, NUM[i - (1 if neutral else 0)])
        pins += [(x, -60), (x, 60)]
    if k > 1: o += [D(xs[0] - 7, -28, xs[-1] - 7, -28)]
    return pins, o
def c60_family(tag, title, items, therm=True):
    for sid, nm, k, neu in items:
        p, o = c60(k, therm, neu)
        add("%s_%s" % (tag, sid), "%s %s" % (title, nm), "Q", p, o, props(prop("Calibre", "16", "A", "Courant assigné", True), prop("Courbe", "C", "", "Courbe de déclenchement", True)))
G("Disjoncteurs C60"); c60_family("c60", "Disjoncteur C60", [("2", "2P", 2, False), ("3", "3P", 3, False), ("1", "1P", 1, False)])
G("Disjoncteurs C60a"); c60_family("c60a", "Disjoncteur C60a", [("2", "2P", 2, False), ("4", "4P", 4, False), ("3", "3P", 3, False), ("1", "1P", 1, False)])
G("Disjoncteurs C60H"); c60_family("c60h", "Disjoncteur C60H", [("2", "2P", 2, False), ("4", "4P", 4, False), ("3", "3P", 3, False)])
G("Disjoncteurs C60LMA"); c60_family("c60l", "Disjoncteur magnétique C60LMA", [("2", "2P", 2, False), ("3", "3P", 3, False)], therm=False)
G("Disjoncteurs C60N"); c60_family("c60n", "Disjoncteur C60N", [("2", "2P", 2, False), ("4", "4P", 4, False), ("3", "3P", 3, False), ("1", "1P", 1, False), ("1n", "1P + N", 2, True)])

G("Disjoncteurs Magnéto-Thermiques")
for sid, nm, kw in [("2", "Disjoncteur magnéto-thermique 2P à commande manuelle", dict(k=2, style="C", hand=True)),
                    ("2d", "Disjoncteur différentiel 2P à commande manuelle", dict(k=2, style="A", hand=True)),
                    ("3", "Disjoncteur magnéto-thermique 3P à commande manuelle", dict(k=3, style="C", hand=True))]:
    p, o = breaker(**kw); add("dm_" + sid, nm, "Q", p, o, props(prop("Calibre", "16", "A", "Courant assigné", True)))

G("Disjoncteurs Moteurs")
p, o = sec_multi(NUM[:3], hand=True); add("dmo_sf", "Sectionneur porte-fusibles 3P à poignée", "Q", p, o)
p, o = gv2(True); add("dmo_gvm", "Disjoncteur moteur 3P (magnétique I>>)", "Q", p, o, props(prop("Réglage", "4 à 6,3", "A", "Plage de réglage", True)))
p, o = breaker(3, "C", hand=True); add("dmo_mt", "Disjoncteur moteur magnéto-thermique 3P à commande manuelle", "Q", p, o)
p, o = gv2(); add("dmo_gv", "Disjoncteur moteur magnéto-thermique 3P", "Q", p, o, props(prop("Réglage", "4 à 6,3", "A", "Plage de réglage thermique", True)))

# ------------------------------------------------------------------ electrofreins / electrovannes
G("Electrofreins")
def brake(id, name, lifted):
    o = [L(-10, -40, -10, -14), L(10, -40, 10, -14), R(-14, -14, 28, 36), L(0, 22, 0, 30), Q(-10, 30, 10, 30, 16, 36 if not lifted else 34, -16, 36 if not lifted else 34),
         D(-30, 38 if not lifted else 40, 30, 38 if not lifted else 40), TL(-6, -30, 7, "A1"), TL(14, -30, 7, "A2")]
    add(id, name, "Y", [(-10, -40), (10, -40)], o)
brake("ef_1", "Électrofrein (frein serré)", False)
brake("ef_2", "Électrofrein (frein desserré)", True)
add("ef_3", "Frein à disque", "Y", [(0, -40), (0, 40)], [L(0, -40, 0, -30), R(-8, -30, 16, 60), L(0, 30, 0, 40), Q(12, -30, 40, -22, 40, 22, 12, 30)])

G("Electrovannes")
def bow(x, y, vertical=False):
    if vertical: return [Q(x - 6, y - 8, x + 6, y - 8, x, y), Q(x - 6, y + 8, x + 6, y + 8, x, y)]
    return [Q(x - 8, y - 6, x - 8, y + 6, x, y), Q(x + 8, y - 6, x + 8, y + 6, x, y)]
add("ev_1", "Électrovanne (vanne à droite)", "Y", [(0, -40), (0, 40)], [L(0, -40, 0, -14), R(-22, -14, 44, 28), L(0, 14, 0, 40)] + bow(32, 0, True) + [L(22, 0, 32, 0), TL(6, -30, 8, "1"), TL(6, 32, 8, "2")])
add("ev_2", "Électrovanne (vanne à gauche)", "Y", [(0, -40), (0, 40)], [L(0, -40, 0, -14), R(-22, -14, 44, 28), L(0, 14, 0, 40)] + bow(-32, 0, True) + [L(-22, 0, -32, 0), TL(6, -30, 8, "1"), TL(6, 32, 8, "2")])
add("ev_3", "Électrovanne (grand modèle)", "Y", [(0, -60), (0, 60)], [L(0, -60, 0, -25), R(-40, -25, 80, 50), L(0, 25, 0, 60)] + bow(54, 0, True) + [L(40, 0, 54, 0)])
add("ev_4", "Électrovanne 2 bobines (Y1 Y2 / N)", "Y", [(-10, -40), (10, -40), (-10, 40)], [L(-10, -40, -10, -14), L(10, -40, 10, -14), R(-22, -14, 44, 28), L(-10, 14, -10, 40)] + bow(-34, 0) + [L(-22, 0, -26, 0), TL(-6, -30, 8, "Y1"), TL(14, -30, 8, "Y2"), TL(-6, 32, 8, "N")])
add("ev_5", "Électrovanne proportionnelle (0/10 V, +24 V, 0)", "Y", [(0, -40), (0, 40), (20, 40)], [L(0, -40, 0, -14), R(-22, -14, 44, 28), L(0, 14, 0, 40), L(20, 14, 20, 40)] + bow(-34, 0) + [L(-22, 0, -26, 0), TL(4, -30, 8, "0/10"), TR(-4, 32, 8, "+24"), TL(24, 32, 8, "0")])
add("ev_m", "Vanne motorisée", "Y", [(0, -40), (0, 40)], [L(0, -40, 0, -26), DP(-10, -26, 10, -26, 10, 26, -10, 26, -10, -26), C(0, 0, 8), L(-4, -2, 4, -2), L(-4, 2, 4, 2), L(0, 26, 0, 40)])

# ------------------------------------------------------------------ fusibles
G("Fusibles")
p, o = sec_multi([("N", "N"), ("1", "2")], nfirst=True); add("fu_1n", "Sectionneur porte-fusible Ph + N", "F", p, o)
p, o = sec_multi([("1", "2")]); add("fu_1", "Sectionneur porte-fusible 1P", "F", p, o)
p, o = sec_multi(NUM[:2]); add("fu_2", "Sectionneur porte-fusibles 2P", "F", p, o)
p, o = sec_multi([("N", "N")] + NUM[:3], nfirst=True); add("fu_3n", "Sectionneur porte-fusibles 3P + N", "F", p, o)
p, o = sec_multi(NUM[:3]); add("fu_3", "Sectionneur porte-fusibles 3P", "F", p, o)
add("fu_p", "Fusible à percuteur", "F", [(0, -40), (0, 40)], [L(0, -40, 0, -18), R(-6, -18, 12, 36), RF(-6, -18, 12, 6), L(0, -12, 0, 18), L(0, 18, 0, 40), TL(8, -30, 8, "1"), TL(8, 32, 8, "2")], props(prop("Calibre", "10", "A", "Calibre", True)))
add("fu_f", "Fusible", "F", [(0, -40), (0, 40)], [L(0, -40, 0, -18), R(-6, -18, 12, 36), L(0, -18, 0, 18), L(0, 18, 0, 40), TL(8, -30, 8, "1"), TL(8, 32, 8, "2")], props(prop("Calibre", "10", "A", "Calibre", True)))

# ------------------------------------------------------------------ instruments de mesure
G("Instruments de mesure")
def meter_h(id, name, txt, arrow=False):
    o = [L(-40, 0, -16, 0), C(0, 0, 16), L(16, 0, 40, 0), T(-30, -8, 7, "1"), T(30, -8, 7, "2")]
    o += [L(0, 10, 0, -10), P(-4, -5, 0, -10, 4, -5)] if arrow else [T(0, 0, 14, txt)]
    add(id, name, "P", [(-40, 0), (40, 0)], o)
meter_h("im_a", "Ampèremètre", "A")
meter_h("im_hz", "Fréquencemètre", "Hz")
meter_h("im_g", "Galvanomètre", "", True)
meter_h("im_v", "Voltmètre", "V")
for i in (1, 2, 3):
    add("im_sh%d" % i, "Shunt de mesure E%d - M" % i, "R", [(0, -40), (0, 40)],
        [W(3), P(0, -40, 0, -24, -10, -24, -10, -12, 10, -12, 10, 0, -10, 0, -10, 12, 10, 12, 10, 24, 0, 24, 0, 40), W(2), TR(-8, -34, 9, "E%d" % i), TR(-8, 34, 9, "M")])
add("im_ri1", "Relais à minimum de courant", "K", [(0, -40), (0, 40)], coil(50, 28, "A1", "A2") + [R(-10, -5, 12, 6), L(-18, -2, -10, -2), L(2, -2, 8, -2), TI(14, 4, 9, "<I")], "", "coil")
add("im_ri2", "Relais à maximum de courant", "K", [(0, -40), (0, 40)], coil(50, 28, "A1", "A2") + [R(-10, -5, 12, 6), L(-18, -2, -10, -2), L(2, -2, 8, -2), TI(14, 4, 9, ">I")], "", "coil")
add("im_w", "Wattmètre (I1 I2 U1 U2)", "P", [(-40, -20), (40, -20), (-40, 20), (40, 20)],
    [C(0, 0, 18), T(0, 0, 15, "W"), L(-40, -20, -15, -10), L(40, -20, 15, -10), L(-40, 20, -15, 10), L(40, 20, 15, 10), T(-22, -22, 7, "I1"), T(22, -22, 7, "I2"), T(-22, 23, 7, "U1"), T(22, 23, 7, "U2")])
add("im_tc", "Transformateur de courant (tore)", "T", [(-40, -20), (-40, 20)], [C(10, 0, 22), L(10, -22, 10, 22), P(-40, -20, -24, -20, -10, -9), P(-40, 20, -24, 20, -10, 9), TL(-34, -28, 8, "1"), TL(-34, 28, 8, "2")])

# ------------------------------------------------------------------ Integral 32
G("Integral 32")
def integ(id, name, act, kind, b):
    if kind == "co":
        o = CO(0) + actuator(act, False); pins = co_pins()
    else:
        o = CT(kind == "nc") + lab() + actuator(act, kind == "nc"); pins = [(0, -40), (0, 40)]
    add(id, name, "Q", pins, o, props(bornes(b)))
integ("in_1", "Contact de déclenchement magnétique 05-06-08", "I>", "co", "05-06-08")
integ("in_2", "Contact auxiliaire NO 13-14", "box", "no", "13-14")
add("in_3", "Contact de position Auto / O 15-16-18", "Q", co_pins(), CO() + [L(-44, 0, -56, -18), L(-44, 0, -32, -18), D(-44, 0, -44, -22), D(-42, 0, -5, 0), TR(-58, -24, 8, "AUTO"), TL(-30, -24, 8, "O")], props(bornes("15-16-18")))
integ("in_4", "Contact auxiliaire NO 23-24", "box", "no", "23-24")
integ("in_5", "Contact auxiliaire NC 31-32", "box", "nc", "31-32")
add("in_6", "Contact de signalisation de défaut 95-96-98", "Q", co_pins(), CO() + [R(-52, -24, 22, 48), L(-52, -12, -30, -12), L(-52, 0, -30, 0), L(-52, 12, -30, 12),
    TI(-41, -18, 8, "I>"), TI(-41, -6, 8, "I>"), TI(-41, 6, 8, "U<"), L(-48, 20, -34, 16), D(-30, 0, -5, 0)], props(bornes("95-96-98")))

# ------------------------------------------------------------------ interrupteurs differentiels
G("Inter Différentiel")
def idiff(k, test):
    labels = [("N", "N")] + NUM[:k - 1]
    xs = xs_of(k); o = []; pins = []
    for i, x in enumerate(xs):
        o += [L(x, -60, x, -46), L(x - 6, -46, x + 6, -46), C(x, -41, 4.5), L(x, 2, x - 14, -32), L(x, 2, x, 60),
              TL(x + 7, -55, 8, labels[i][0]), TL(x + 5, 53, 8, labels[i][1])]
        pins += [(x, -60), (x, 60)]
    x1, xn = xs[0], xs[-1]
    o += [RR(x1 - 8, 14, (xn - x1) + 16, 12)]
    if not test:
        xl = x1 - 26
        o += [DP(xn - 7, -16, xl, -16, xl, 20, x1 - 8, 20)]
    else:
        xr = xn + 26
        o += [D(x1 - 30, -16, xr - 4, -16), R(xr - 4, -20, 8, 8), L(xr + 4, -16, xr + 8, -16), L(xr + 8, -19, xr + 8, -13), D(xr, -12, xr, 10),
              R(xr - 5, 10, 10, 20), P(xn + 8, 20, xn + 14, 20, xn + 14, 36, xr + 12, 36, xr + 12, 24, xr + 5, 24),
              # circuit de test : poussoir T, resistance R
              L(x1 - 44, -34, x1 - 30, -12), P(x1 - 44, -34, x1 - 44, -44, x1 - 24, -44, x1 - 24, -34) if False else P(x1 - 44, -36, x1 - 44, -46, x1 - 22, -46, x1 - 22, -40),
              L(x1 - 52, -22, x1 - 46, -22), L(x1 - 52, -26, x1 - 52, -18), L(x1 - 52, -26, x1 - 48, -26), L(x1 - 52, -18, x1 - 48, -18), T(x1 - 50, -34, 8, "T"),
              P(x1 - 30, -12, x1 - 30, -6, x1 - 44, -6, x1 - 44, 6), R(x1 - 52, 6, 16, 18), T(x1 - 44, 15, 9, "R"), P(x1 - 44, 24, x1 - 44, 40, xn, 40), F(xn, 40, 2), P(x1 - 22, -40, x1 - 22, -8 if False else -40)]
    return pins, o
p, o = idiff(2, True); add("id_2t", "Interrupteur différentiel 2P avec circuit de test", "Q", p, o, props(prop("Sensibilité", "30", "mA", "Sensibilité", True)))
p, o = idiff(4, True); add("id_4t", "Interrupteur différentiel 4P avec circuit de test", "Q", p, o, props(prop("Sensibilité", "30", "mA", "Sensibilité", True)))
p, o = idiff(2, False); add("id_2", "Interrupteur différentiel 2P", "Q", p, o, props(prop("Sensibilité", "30", "mA", "Sensibilité", True)))
p, o = idiff(4, False); add("id_4", "Interrupteur différentiel 4P", "Q", p, o, props(prop("Sensibilité", "30", "mA", "Sensibilité", True)))

# ------------------------------------------------------------------ interrupteurs - sectionneurs
G("Interrupteurs - Sectionneurs")
add("is_pont", "Pont de sectionnement 1-2", "X", [(0, -40), (0, 40)], [L(0, -40, 0, -24), A(-7, -28, 14, 10, 0, 180), W(4), L(0, -20, 0, 20), W(2), A(-7, 18, 14, 10, 180, 180), L(0, 24, 0, 40), TL(8, -32, 9, "1"), TL(8, 33, 9, "2")])
p, o = sec_multi([None, None, None], hand=True); add("is_sf3", "Sectionneur porte-fusibles 3P à poignée", "Q", p, o)
p, o = sec_multi(NUM[:2], fuse=False, circle=True); add("is_2", "Interrupteur-sectionneur 2P", "Q", p, o)
p, o = sec_multi(NUM[:4], fuse=False, circle=True); add("is_4", "Interrupteur-sectionneur 4P", "Q", p, o)
p, o = sec_multi(NUM[:3], fuse=False, circle=True); add("is_3", "Interrupteur-sectionneur 3P", "Q", p, o)
p, o = sec_multi(NUM[:4], hand=True); add("is_sf4", "Sectionneur porte-fusibles 4P à poignée", "Q", p, o)
p, o = breaker(2, "C", hand=True); add("is_d2", "Disjoncteur 2P à commande manuelle", "Q", p, o)
p, o = breaker(2, "A", hand=True); add("is_dd2", "Disjoncteur différentiel 2P à commande manuelle", "Q", p, o)
p, o = sec_multi([("N", "N")] + NUM[:3], nfirst=True); add("is_sfn", "Sectionneur porte-fusibles 3P + N", "Q", p, o)
p, o = sec_multi([("N", "N")] + NUM[:3], fuse=False); add("is_3n", "Sectionneur 3P + N", "Q", p, o)
p, o = breaker(3, "C", hand=True, iso=True); add("is_d3", "Disjoncteur 3P à commande manuelle", "Q", p, o)
p, o = sec_multi(NUM[:3]); add("is_sf", "Sectionneur porte-fusibles 3P", "Q", p, o)
def hsect(k):
    ys = xs_of(k, 40); o = []; pins = []
    for i, y in enumerate(ys):
        j = k - 1 - i
        o += [L(-40, y, -20, y), C(-17, y, 2.5), L(-20, y - 6, -20, y + 6), L(26, y, -16, y + 16), L(26, y, 40, y), T(-30, y - 9, 9, str(2 * j + 1)), T(32, y - 9, 9, str(2 * j + 2))]
        pins += [(-40, y), (40, y)]
    o += [D(5, ys[0] + 8, 5, ys[-1] + 8)]
    return pins, o
p, o = hsect(4); add("is_h4", "Interrupteur-sectionneur 4P (horizontal)", "Q", p, o)
p, o = hsect(3); add("is_h3", "Interrupteur-sectionneur 3P (horizontal)", "Q", p, o)

# ------------------------------------------------------------------ masse - terre
G("Masse - Terre")
add("mt_pont", "Barrette de coupure de terre 1-2", "X", [(0, -40), (0, 40)], [L(0, -40, 0, -16), A(-8, -16, 16, 12, 0, 180), A(-8, 4, 16, 12, 180, 180), L(0, 16, 0, 40), TL(8, -32, 9, "1"), TL(8, 33, 9, "2")])
add("mt_ch", "Masse (châssis)", "X", [(0, -40)], [L(0, -40, 0, 0), W(5), L(-26, 0, 26, 0), W(3)] + [L(x, 2, x - 8, 14) for x in (-18, -8, 2, 12, 22)] + [W(2)])
add("mt_m", "Masse", "X", [(0, -40)], [L(0, -40, 0, 0), W(5), L(-26, 0, 26, 0), W(2)])
add("mt_t", "Terre", "X", [(0, -40)], [L(0, -40, 0, 0), L(-26, 0, 26, 0), L(-17, 8, 17, 8), L(-8, 16, 8, 16)])

# ------------------------------------------------------------------ moteurs
def winding_h(x0, x1, y, k=4):
    step = (x1 - x0) / float(k)
    return [A(x0 + i * step, y - step / 2.0, step, step, 0, 180) for i in range(k)]
def winding_v(x, y0, y1, k=4):
    step = (y1 - y0) / float(k)
    return [A(x - step / 2.0, y0 + i * step, step, step, 90, -180) for i in range(k)]
def motor_top(id, name, t1, t2, labels, r=26, double=False, pe=False, bottom=None, pref="M", pitch=40):
    k = len(labels); xs = xs_of(k, pitch); o = [C(0, 0, r)]; pins = []
    if double: o += [C(0, 0, r - 5)]
    o += [T(0, -6, 15, t1), T(0, 11, 13, t2)]
    for x, l in zip(xs, labels):
        tx = x * 0.42; ty = -math.sqrt(max(r * r - tx * tx, 1))
        o += [P(x, -60, x, -46, tx, ty), TL(x + 4, -54, 7, l)]
        pins.append((x, -60))
    if bottom:
        for x, l in zip(xs, bottom):
            tx = x * 0.42; ty = math.sqrt(max(r * r - tx * tx, 1))
            o += [P(x, 60, x, 46, tx, ty), TL(x + 4, 54, 7, l)]
            pins.append((x, 60))
    if pe:
        px = xs[-1] + 40
        o += [L(px, -60, px, 30), TL(px + 4, -54, 7, "PE"), "DD %s 0 %s 0" % (n(r), n(px)), L(px - 10, 30, px + 10, 30), L(px - 6, 35, px + 6, 35), L(px - 2, 40, px + 2, 40)]
        pins.append((px, -60))
    add(id, name, pref, pins, o, props(prop("Puissance", "1,5", "kW", "Puissance utile", True)))
G("Moteurs (norme IEC60617)")
add("mi_cc", "Moteur à courant continu", "M", [(-40, -60), (40, -60)],
    [C(0, 0, 26), T(0, -6, 15, "M"), L(-10, 8, 10, 8), D(-10, 13, 10, 13), P(-40, -60, -40, 0, -26, 0), P(40, -60, 40, 0, 26, 0), TL(-36, -54, 7, "1"), TL(44, -54, 7, "2")])
add("mi_1s", "Moteur monophasé série", "M", [(-40, -60), (40, -60)],
    [C(0, 10, 24), T(0, 5, 14, "M"), T(0, 20, 12, "1~"), P(-40, -60, -40, -34, -24, -34)] + winding_h(-24, 24, -34, 4) + [P(24, -34, 40, -34, 40, 10, 24, 10), P(-24, 10, -40, 10, -40, -20) if False else P(-24, 10, -34, 10, -34, -20, 40, -20, 40, -60), T(0, -42, 8, "~")])
add("mi_1", "Moteur monophasé 4 bornes", "M", [(-20, -60), (20, -60), (60, -10), (60, 10)],
    [C(0, 0, 30), T(0, -6, 15, "M"), T(0, 12, 13, "1~"), P(-20, -60, -20, -22), P(20, -60, 20, -22), L(29, -8, 60, -10), L(29, 8, 60, 10), TL(-16, -54, 7, "1"), TL(24, -54, 7, "2"), T(52, -18, 7, "3"), T(52, 18, 7, "4")])
add("mi_1c", "Moteur monophasé à enroulement auxiliaire", "M", [(-40, -60), (60, -60)],
    [C(0, 0, 28), T(0, -6, 15, "M"), T(0, 12, 13, "1~"), P(-40, -60, -40, 0, -28, 0), L(60, -60, 60, -44)] + winding_v(60, -44, -12, 4) + [P(60, -12, 60, 0, 28, 0), L(-40, -60, 60, -60) if False else L(-40, -52, -40, -52)])
add("mi_ms", "Moteur synchrone monophasé", "M", [(-20, -60), (20, -60)],
    [C(0, -6, 24), T(0, -12, 14, "MS"), T(0, 4, 12, "1~"), L(-20, -60, -20, -20), L(20, -60, 20, -20), P(-20, 30, -20, 40)] + winding_h(-20, 20, 40, 4) + [P(20, 40, 20, 30), L(-10, 48, 10, 48), D(-10, 53, 10, 53)])
motor_top("mi_3a", "Moteur triphasé (U V W)", "M", "3 ~", ["U", "V", "W"], r=30, double=True)
motor_top("mi_3b", "Moteur triphasé (simple cercle)", "M", "3 ~", ["U", "V", "W"], r=30)
motor_top("mi_3c", "Moteur triphasé (1 2 3)", "M", "3~", ["1", "2", "3"], r=26, double=True, pitch=20)
motor_top("mi_gs", "Alternateur triphasé GS", "GS", "3~", ["1", "2", "3"], r=26, pref="G", pitch=20)
add("mi_g", "Génératrice à courant continu à excitation séparée", "G", [(-60, -60), (60, -60)],
    [C(0, 0, 24), T(0, -5, 14, "G"), L(-9, 8, 9, 8), D(-9, 13, 9, 13), RF(-34, -4, 10, 8), RF(24, -4, 10, 8), P(-60, -60, -60, 0, -34, 0), P(34, 0, 60, 0, 60, -20)] + winding_v(60, -52, -20, 4) + [L(60, -60, 60, -52)])

G("Moteurs à Courant Continu")
def dc_h(id, name, t, pref):
    add(id, name, pref, [(-60, 0), (60, 0)],
        [C(0, 0, 22), T(0, -3, 16, t), L(-8, 12, 8, 12) if t != "DT" else "", RF(-32, -4, 10, 8), RF(22, -4, 10, 8), L(-60, 0, -32, 0), L(32, 0, 60, 0),
         T(-50, -9, 8, "1"), T(50, -9, 8, "2"), T(-46, 11, 10, "+"), T(46, 11, 10, "-")])
dc_h("mc_dt", "Dynamo tachymétrique", "DT", "G")
dc_h("mc_g", "Génératrice à courant continu", "G", "G")
add("mc_l", "Inducteur (enroulement d'excitation)", "L", [(-60, 0), (60, 0)], [L(-60, 0, -24, 0)] + winding_h(-24, 24, 0, 4) + [L(24, 0, 60, 0), T(-46, -9, 8, "1"), T(46, -9, 8, "2")])
add("mc_ls", "Inducteur série", "L", [(-60, 0), (60, 0)], [L(-60, 0, -40, 0)] + winding_h(-40, 40, 0, 8) + [L(40, 0, 60, 0), T(-52, -9, 8, "1"), T(52, -9, 8, "2")])
dc_h("mc_m", "Moteur à courant continu", "M", "M")

G("Moteurs Triphasés")
motor_top("m3_6", "Moteur triphasé 6 bornes (1U 1V 1W / 2U 2V 2W)", "M", "3 ~", ["1U", "1V", "1W"], r=28, bottom=["2U", "2V", "2W"])
motor_top("m3_uvw", "Moteur triphasé U V W", "M", "3 ~", ["U", "V", "W"], r=30, double=True)
motor_top("m3_pe", "Moteur triphasé U V W + PE", "M", "3 ~", ["U", "V", "W"], r=26, double=True, pe=True)
motor_top("m3_123", "Moteur triphasé 1 2 3 + terre", "M", "3 ~", ["1", "2", "3"], r=26, double=True, pe=True)

# ------------------------------------------------------------------ photovoltaique
G("Photovoltaïque")
add("pv_ond", "Onduleur photovoltaïque", "T", [(-60, 0), (60, 0)], [R(-36, -36, 72, 72), L(-36, 36, 36, -36), L(-60, 0, -36, 0), L(36, 0, 60, 0), T(-16, -14, 18, "="), T(16, 16, 18, "~"), T(-48, -9, 8, "1"), T(48, -9, 8, "2")])
add("pv_pan", "Module photovoltaïque", "G", [(0, -60), (0, 60)], [R(-16, -36, 32, 72), P(-16, -36, 0, -14, 16, -36), L(0, -60, 0, -36), L(0, 36, 0, 60), TL(5, -50, 9, "+"), TL(5, 50, 9, "-")])
add("pv_con", "Connecteur photovoltaïque", "X", [(0, -60), (0, 60)], [L(0, -60, 0, -30), R(-12, -30, 24, 44), L(0, -30, 0, -6), P(-7, -14, 0, -6, 7, -14), L(-12, 0, 12, 0) if False else L(0, 14, 0, 60), R(-16, 14, 32, 14), C(-8, 21, 3), C(8, 21, 3)])
add("pv_ch", "Chaîne de modules (raccordement en Y)", "X", [(0, -60), (0, 60)], [L(0, -60, 0, -10), W(3), P(-22, -32, 0, -10, 22, -32), P(-22, -14, 0, 8, 22, -14), W(2), L(0, 8, 0, 60)])
def dc_brk(id, name, diff):
    o = [L(0, -60, 0, -44), L(-3, -47, 3, -41), L(-3, -41, 3, -47), L(0, 10, -14, -40), P(0, 10, 0, 22, 8, 22, 8, 30, 0, 30), A(-6, 30, 12, 12, 90, -180), L(0, 42, 0, 60)]
    if diff: o += [DP(-7, -16, -24, -16, -24, 16, -12, 16), RR(-12, 11, 24, 10)]
    add(id, name, "Q", [(0, -60), (0, 60)], o)
dc_brk("pv_dj", "Disjoncteur courant continu 1P", False)
dc_brk("pv_dd", "Disjoncteur différentiel courant continu 1P", True)
add("pv_sec", "Interrupteur-sectionneur courant continu", "Q", [(0, -60), (0, 60)], [L(0, -60, 0, -42), L(-6, -42, 6, -42), C(0, -38, 3), W(3), L(0, 16, -16, -34), W(2), L(0, 16, 0, 60)])

G("Piles et batterie")
add("pi_bat", "Batterie (boîtier)", "G", [(-40, -20), (40, -20)], [R(-50, -20, 100, 60), R(-46, -16, 12, 10), R(34, -16, 12, 10), T(-34, 0, 12, "-"), T(28, 0, 12, "+")])
add("pi_1", "Pile (1 élément)", "G", [(-40, 0), (40, 0)], [L(-40, 0, -6, 0), L(-6, -20, -6, 20), RF(4, -10, 8, 20), L(12, 0, 40, 0), T(-22, 12, 14, "+"), T(26, 12, 14, "-"), T(-30, -9, 9, "1"), T(30, -9, 9, "2")])
add("pi_2", "Pile (2 éléments)", "G", [(-40, 0), (40, 0)], [L(-40, 0, -14, 0), L(-14, -16, -14, 16), RF(-8, -8, 6, 16), L(-2, 0, 4, 0), L(4, -16, 4, 16), RF(10, -8, 6, 16), L(16, 0, 40, 0), T(-26, 12, 12, "+"), T(28, 12, 12, "-"), T(-32, -9, 8, "1"), T(32, -9, 8, "2")])

# ------------------------------------------------------------------ poussoirs et commutateurs
G("Poussoirs et commutateurs")
pb("pc_cab", "Arrêt d'urgence à câble NC 1-2", True, "cable", "1-2")
pb("pc_cp_nc", "Bouton coup de poing NC 1-2", True, "mush", "1-2")
pb("pc_cp_no", "Bouton coup de poing NO 3-4", False, "mush", "3-4")
pb("pc_lum_nc", "Bouton poussoir lumineux NC 1-2", True, "lum", "1-2")
pb("pc_lum_no", "Bouton poussoir lumineux NO 3-4", False, "lum", "3-4")
pb("pc_bp_nc", "Bouton poussoir NC 1-2", True, "E", "1-2")
pb("pc_bp_no", "Bouton poussoir NO 3-4", False, "E", "3-4")
pb("pc_k_nc", "Commutateur à clé NC 1-2", True, "selkey", "1-2")
pb("pc_k_no", "Commutateur à clé NO 3-4", False, "selkey", "3-4")
pb("pc_ped_nc", "Pédale NC 1-2", True, "ped", "1-2")
pb("pc_ped_no", "Pédale NO 3-4", False, "ped", "3-4")
pb("pc_rot_nc", "Bouton tournant NC 21-22", True, "rot", "21-22")
pb("pc_rot_no", "Bouton tournant NO 13-14", False, "rot", "13-14")
o = NC(0) + lab(0, "1", "2") + NO(60) + lab(60, "3", "4") + actuator("rot", True, 0) + [D(8, 0, 49, 0)]
add("pc_rot2", "Bouton tournant NC + NO", "S", [(0, -40), (0, 40), (60, -40), (60, 40)], o)
pb("pc_sel_nc", "Commutateur 2 positions NC 1-2", True, "sel", "1-2")
pb("pc_sel_no", "Commutateur 2 positions NO 3-4", False, "sel", "3-4")
o = NO(0) + lab(0, "3", "4") + NC(60) + lab(60, "1", "2") + actuator("sel", False, 0) + [D(-8, 0, 66, 0)]
add("pc_sel2", "Commutateur 2 positions NO + NC", "S", [(0, -40), (0, 40), (60, -40), (60, 40)], o)
o = NO(0) + lab(0, "3", "4") + NO(60) + lab(60, "13", "14") + actuator("sel", False, 0) + [D(-8, 0, 49, 0), T(-46, -12, 7, "1"), T(-39, -12, 7, "0"), T(-32, -12, 7, "2")]
add("pc_sel3", "Commutateur 3 positions 2 NO", "S", [(0, -40), (0, 40), (60, -40), (60, 40)], o)
pb("pc_tir_nc", "Bouton à tirette NC 1-2", True, "tir", "1-2")
pb("pc_tir_no", "Bouton à tirette NO 3-4", False, "tir", "3-4")
pb("pc_vol_nc", "Commande par volant NC 1-2", True, "wheel", "1-2")
pb("pc_vol_no", "Commande par volant NO 3-4", False, "wheel", "3-4")
add("pc_pot", "Potentiomètre de consigne", "R", [(0, -40), (0, 40), (-60, 0)], [L(0, -40, 0, -26), R(-9, -26, 18, 52), L(0, 26, 0, 40), L(-60, 0, -22, 0), K(-22, -6, -22, 6, -9, 0), TL(6, -34, 8, "3"), TL(6, 34, 8, "14"), T(-50, -9, 8, "9")])

# ------------------------------------------------------------------ prises
G("Prises de courant")
def earth(x, y): return [L(x - 10, y, x + 10, y), L(x - 6, y + 5, x + 6, y + 5), L(x - 2, y + 10, x + 2, y + 10)]
add("pr_2t", "Prise 2P + T", "X", [(-10, -60), (10, -60)], [C(0, 0, 22), P(-10, -60, -10, -8), P(10, -60, 10, -8), A(-15, -8, 10, 10, 0, 180), A(5, -8, 10, 10, 0, 180), L(0, 22, 0, 34)] + earth(0, 34) + [TL(-6, -44, 8, "P"), TL(14, -44, 8, "N"), T(0, 12, 8, "PE")])
add("pr_f", "Socle de prise 2P + T (vue de face)", "X", [(-20, -60), (20, -60), (0, 60)], [R(-30, -40, 60, 80), C(0, 0, 22), F(0, -12, 4), C(-9, 6, 3.5), C(9, 6, 3.5), L(-20, -60, -20, -40), L(20, -60, 20, -40), L(0, 40, 0, 60)])
add("pr_r", "Prise ronde 2P + T", "X", [(-20, -60), (20, -60), (0, 60)], [C(0, 0, 30), F(-12, -4, 5), F(12, -4, 5), C(0, 16, 4), P(-20, -60, -20, -22), P(20, -60, 20, -22), L(0, 20, 0, 60), TL(6, 50, 9, "PE")])
add("pr_r3", "Prise ronde 3 broches", "X", [(-20, -60), (20, -60)], [C(0, 0, 30), C(-12, -6, 4), C(12, -6, 4), C(0, 22, 4), P(-20, -60, -20, -22), P(20, -60, 20, -22)])
add("pr_4", "Prise 3P + N + T", "X", [(-60, -60), (-20, -60), (20, -60), (60, -60)],
    [C(0, 0, 30), C(0, -18, 3.5), C(-17, -6, 3.5), C(17, -6, 3.5), C(-11, 14, 3.5), C(11, 14, 3.5), P(-60, -60, -60, -40, -24, -18), P(-20, -60, -20, -40, -8, -29), P(20, -60, 20, -40, 8, -29), P(60, -60, 60, -40, 24, -18),
     TL(-56, -54, 8, "N"), TL(-16, -54, 8, "L1"), TL(24, -54, 8, "L2"), TL(64, -54, 8, "L3"), L(0, 30, 0, 40)] + earth(0, 40))
add("pr_3", "Prise 3P + T", "X", [(-40, -60), (0, -60), (40, -60)],
    [C(0, 0, 30), C(-15, -10, 3.5), C(15, -10, 3.5), C(-15, 10, 3.5), C(15, 10, 3.5), C(0, 22, 4), P(-40, -60, -40, -40, -20, -22), L(0, -60, 0, -30), P(40, -60, 40, -40, 20, -22),
     TL(-36, -54, 8, "L1"), TL(4, -54, 8, "L2"), TL(44, -54, 8, "L3")])

# ------------------------------------------------------------------ protections
G("Protections électriques")
def arrow_d(x, y0, y1): return [L(x, y0, x, y1), P(x - 4, y1 - 6, x, y1, x + 4, y1 - 6)]
def arrow_u(x, y0, y1): return [L(x, y0, x, y1), P(x - 4, y1 + 6, x, y1, x + 4, y1 + 6)]
add("pe_ecd", "Éclateur double", "F", [(0, -40), (0, 40)], arrow_d(0, -40, -12) + arrow_u(0, 40, 12) + [L(-7, 0, 7, 0), TL(6, -30, 9, "1"), TL(6, 32, 9, "2")])
add("pe_ec", "Éclateur", "F", [(0, -40), (0, 40)], arrow_d(0, -40, -6) + arrow_u(0, 40, 6) + [TL(6, -30, 9, "1"), TL(6, 32, 9, "2")])
add("pe_pf", "Parafoudre (quadripôle)", "F", [(-40, -10), (40, -10), (-40, 10), (40, 10)],
    [R(-16, -22, 32, 44), L(-40, -10, -16, -10), L(40, -10, 16, -10), L(-40, 10, -16, 10), L(40, 10, 16, 10), W(3), P(4, -14, -4, 2, 4, 2, -4, 16), W(2),
     T(-28, -19, 9, "1"), T(28, -19, 9, "1"), T(-28, 19, 9, "2"), T(28, 19, 9, "2")])
add("pe_pft", "Parafoudre N - L1 avec terre", "F", [(-20, -40), (20, -40), (20, 40)],
    [R(-30, -24, 60, 40), L(-20, -40, -20, -24), L(20, -40, 20, -24), L(20, 16, 20, 40), TL(-16, -32, 7, "N"), TL(24, -32, 7, "L1"), TL(24, 26, 7, "T")] + earth(20, 44))

# ------------------------------------------------------------------ relais magnetiques / thermiques
G("Relais magnétiques")
add("rm_1", "Relais magnétique (1-3-5 / 2-4-6)", "F", [(0, -40), (0, 40)], [L(0, -40, 0, -12), A(-12, -12, 24, 24, 90, -180), L(0, 12, 0, 40), TR(-6, -30, 8, "1-3-5"), TR(-6, 32, 8, "2-4-6")])
add("rm_2", "Relais magnétique 1-2", "F", [(0, -40), (0, 40)], [W(3), L(0, -40, 0, -12), A(-12, -12, 24, 24, 90, -180), L(0, 12, 0, 40), W(2), TL(14, -18, 10, "1"), TL(14, 22, 10, "2")])
add("rm_3", "Relais thermique (1-3-5 / 2-4-6)", "F", [(0, -40), (0, 40)], [P(0, -40, 0, -14, 22, -14, 22, 14, 0, 14, 0, 40), TR(-6, -30, 8, "1-3-5"), TR(-6, 32, 8, "2-4-6")])
G("Relais thermiques")
xs = xs_of(3); o = []
for i, x in enumerate(xs):
    o += [L(x, -40, x, -10), P(x, -10, x - 14, -10, x - 14, 10, x, 10), L(x, 10, x, 40), TL(x + 5, -31, 7, PW[i][0]), TL(x + 5, 32, 7, PW[i][1])]
o += [DP(xs[0] - 26, -18, xs[2] + 14, -18, xs[2] + 14, 18, xs[0] - 26, 18, xs[0] - 26, -18), D(xs[0] - 14, 0, xs[2], 0)]
add("rth_3", "Relais thermique tripolaire", "F", sum([[(x, -40), (x, 40)] for x in xs], []), o, props(prop("Réglage", "4 à 6", "A", "Plage de réglage", True)), "thermal")

# ------------------------------------------------------------------ semiconducteurs
G("Semiconducteurs")
def diode_h(id, name, extra, pref="V"):
    add(id, name, pref, [(-40, 0), (40, 0)], [L(-40, 0, -9, 0), K(-9, -9, -9, 9, 9, 0), L(9, -9, 9, 9), L(9, 0, 40, 0), T(-28, -9, 9, "A"), T(30, -9, 9, "K")] + extra)
add("sc_bi", "Diode bidirectionnelle (transil)", "V", [(-40, 0), (40, 0)], [L(-40, 0, -4, 0), L(4, 0, 40, 0), L(-4, -12, -4, 12), L(4, -12, 4, 12), K(-4, -8, 4, -8 if False else -3, -4, 2) if False else K(4, -11, 4, -1, -4, -6), K(-4, 1, -4, 11, 4, 6), T(-30, -9, 7, "1"), T(30, -9, 7, "2")])
diode_h("sc_sch", "Diode Schottky", [P(5, 9, 5, 12) if False else P(13, -9, 13, -6), P(9, -9, 13, -9), P(9, 9, 5, 9, 5, 6)])
diode_h("sc_var", "Diode à capacité variable", [L(14, -9, 14, 9)])
diode_h("sc_z", "Diode Zener", [P(9, -9, 14, -13), P(9, 9, 4, 13)])
diode_h("sc_d", "Diode", [])
add("sc_opto", "Optocoupleur", "U", [(-20, -40), (-20, 40), (20, -40), (20, 40)],
    [R(-36, -16, 72, 32), L(-20, -40, -20, -7), K(-27, -7, -13, -7, -20, 5), L(-27, 5, -13, 5), L(-20, 5, -20, 40), L(-6, -4, 2, -4), P(-1, -7, 2, -4, -1, -1), L(-6, 3, 2, 3), P(-1, 0, 2, 3, -1, 6),
     L(10, -9, 10, 9), L(10, -3, 20, -10), L(20, -10, 20, -40), L(10, 3, 20, 10), L(20, 10, 20, 40), TL(-16, -30, 7, "A1"), TL(-16, 32, 7, "A2"), TL(24, -30, 7, "13"), TL(24, 32, 7, "14")])
add("sc_led", "Diode électroluminescente", "V", [(0, -40), (0, 40)], [L(0, -40, 0, -9), K(-9, -9, 9, -9, 0, 9), L(-9, 9, 9, 9), L(0, 9, 0, 40), L(13, -4, 21, -10), P(17, -10, 21, -10, 21, -6), L(13, 4, 21, -2), P(17, -2, 21, -2, 21, 2), TL(6, -30, 9, "A"), TL(6, 32, 9, "K")])
add("sc_thy", "Thyristor", "V", [(0, -40), (0, 40), (-20, 40)], [L(0, -40, 0, -9), K(-9, -9, 9, -9, 0, 9), L(-9, 9, 9, 9), L(0, 9, 0, 40), P(-4, 9, -12, 20, -20, 20, -20, 40), TL(6, -30, 9, "A"), TL(6, 32, 9, "K"), TR(-24, 32, 9, "G")])
add("sc_tri", "Triac", "V", [(0, -40), (0, 40), (-20, 40)], [L(0, -40, 0, -8), L(-18, -8, 18, -8), L(-18, 8, 18, 8), K(-18, 8, -2, 8, -10, -8), K(2, -8, 18, -8, 10, 8), L(0, 8, 0, 40), P(-10, 8, -20, 22, -20, 40), TL(6, -30, 9, "A2"), TL(6, 32, 9, "A1"), TR(-24, 32, 9, "G")])

# ------------------------------------------------------------------ sondes
G("Sondes")
add("so_r3", "Sonde résistive 3 fils", "B", [(40, -40), (40, 20), (40, 40)], [R(-7, -24, 14, 48), P(-16, 26, -10, 26, 12, -26), P(0, -24, 0, -40, 40, -40), P(0, 24, 0, 40, 40, 40), P(0, 30, 20, 30, 20, 20, 40, 20) if False else P(0, 32, 26, 32, 26, 20, 40, 20), T(34, -47, 7, "1"), T(34, 13, 7, "2"), T(34, 47, 7, "3")])
add("so_2", "Sonde à deux électrodes", "B", [(0, -40), (0, 40)], [L(0, -40, 0, -22), W(3), R(-24, -22, 48, 44), W(2), L(0, 22, 0, 40), RF(-6, -12, 10, 3), RF(-6, -5, 10, 3), L(4, -10, 12, -10), L(4, -4, 12, -4),
     A(-14, 2, 9, 9, 0, 180), A(-5, 2, 9, 9, 180, 180), A(4, 2, 9, 9, 0, 180)])

# ------------------------------------------------------------------ transformateurs
G("Transformateurs")
def tr2(id, name, top, bot, ground=False, r=22):
    o = [C(0, -13, r), C(0, 13, r)]; pins = []
    for xs, lbl, sgn in ((xs_of(len(top)), top, -1), (xs_of(len(bot)), bot, 1)):
        for x, l in zip(xs, lbl):
            tx = x if abs(x) < r else x * 0.5
            ty = sgn * (13 + math.sqrt(max(r * r - tx * tx, 0)))
            o += [P(x, sgn * 60, x, sgn * 48, tx, ty) if tx != x else L(x, sgn * 60, x, ty), TL(x + 4, sgn * 54, 7, l)]
            pins.append((x, sgn * 60))
    if ground: o += [P(-r, 13, -40, 13, -40, 30)] + earth(-40, 30)
    add(id, name, "T", pins, o, props(prop("Puissance", "100", "VA", "Puissance apparente", True)))
tr2("tr_m", "Transformateur monophasé (1 2 / 3 4)", ["1", "2"], ["3", "4"])
tr2("tr_3", "Transformateur triphasé (A1 B1 C1 / N A2 B2 C2)", ["A1", "B1", "C1"], ["N", "A2", "B2", "C2"])
tr2("tr_3b", "Transformateur triphasé (A1 B1 C1 / A2 B2 C2)", ["A1", "B1", "C1"], ["A2", "B2", "C2"])
tr2("tr_me", "Transformateur monophasé avec écran à la terre", ["1", "2"], ["3", "4"], ground=True)
add("tr_auto", "Autotransformateur à prises", "T", [(0, -60), (0, 60), (20, -30), (20, 0), (20, 30)],
    [L(0, -60, 0, -48)] + winding_v(0, -48, 48, 8) + [L(0, 48, 0, 60), L(-10, -48, -10, 48), L(6, -30, 20, -30), L(6, 0, 20, 0), L(6, 30, 20, 30)])
add("tr_box", "Transformateur redresseur 0-230-400 V", "T", [(-20, -60), (0, -60), (20, -60), (-10, 60), (10, 60), (-40, 60)],
    [R(-30, -40, 60, 80), L(-20, -60, -20, -40), L(0, -60, 0, -40), L(20, -60, 20, -40), L(-10, 40, -10, 60), L(10, 40, 10, 60),
     T(-20, -33, 7, "0V"), T(0, -33, 7, "230V"), T(20, -33, 7, "400V"), T(-10, 33, 7, "-"), T(10, 33, 7, "+"),
     A(-20, -6, 10, 10, 0, 180), A(-10, -6, 10, 10, 180, 180), L(2, -12, -4, 12), L(6, -3, 22, -3), D(6, 4, 22, 4), L(-40, 60, -40, 44)] + earth(-40, 44) if False else
    [R(-30, -40, 60, 80), L(-20, -60, -20, -40), L(0, -60, 0, -40), L(20, -60, 20, -40), L(-10, 40, -10, 60), L(10, 40, 10, 60),
     T(-20, -33, 7, "0V"), T(0, -33, 7, "230V"), T(20, -33, 7, "400V"), T(-10, 33, 7, "-"), T(10, 33, 7, "+"),
     A(-20, -6, 10, 10, 0, 180), A(-10, -6, 10, 10, 180, 180), L(2, -12, -4, 12), L(6, -3, 22, -3), D(6, 4, 22, 4), P(-40, 60, -40, 50)] + [L(-50, 50, -30, 50), L(-46, 45, -34, 45), L(-42, 40, -38, 40)])
add("tr_3w", "Transformateur à trois enroulements", "T", [(-10, -60), (10, -60), (-30, 60), (-14, 60), (14, 60), (30, 60)],
    [C(0, -12, 22), C(-14, 12, 22), C(14, 12, 22), L(-10, -60, -10, -32), L(10, -60, 10, -32), L(-30, 60, -30, 28), L(-14, 60, -14, 34), L(14, 60, 14, 34), L(30, 60, 30, 28)])
add("tr_tc", "Transformateur de courant", "T", [(-40, -20), (-40, 20)], [C(10, 0, 24), P(-40, -20, -26, -20, -10, -12), P(-40, 20, -26, 20, -10, 12), TL(-34, -29, 9, "1"), TL(-34, 29, 9, "2")])
add("tr_w", "Transformateur à enroulements séparés (noyau)", "T", [(-30, -40), (-30, 40), (30, -40), (30, 40)],
    [L(-30, -40, -30, -32)] + [A(-36, -32 + 16 * i, 12, 16, 90, -180) for i in range(4)] + [L(-30, 32, -30, 40), L(30, -40, 30, -32)] + [A(24, -32 + 16 * i, 12, 16, 90, 180) for i in range(4)] + [L(30, 32, 30, 40), W(3), L(-4, -34, -4, 34), L(4, -34, 4, 34), W(2)])

# ------------------------------------------------------------------ unifilaire
G("Unifilaire - Conducteurs")
def uni_cond(id, name, k, circle=False, thick=False):
    o = [L(0, -40, 0, 40), W(3)]
    for i in range(k):
        y = (i - (k - 1) / 2.0) * 7
        o += [L(-18, y + 9, 18, y - 9)]
    o += [W(2)]
    if circle: o += [C(22, (-(k - 1) / 2.0) * 7 - 11, 4)]
    add(id, name, "W", [(0, -40), (0, 40)], o)
uni_cond("uc_1", "Conducteur - 1 fil", 1)
uni_cond("uc_2n", "Conducteurs - 2 fils dont neutre", 2, True)
uni_cond("uc_3n", "Conducteurs - 3 fils dont neutre", 3, True)
uni_cond("uc_2", "Conducteurs - 2 fils", 2)
uni_cond("uc_4n", "Conducteurs - 4 fils dont neutre", 4, True)
uni_cond("uc_5n", "Conducteurs - 5 fils dont neutre", 5, True)
uni_cond("uc_3", "Conducteurs - 3 fils", 3)
uni_cond("uc_4", "Conducteurs - 4 fils", 4)
uni_cond("uc_1n", "Conducteur neutre", 1, True)
add("uc_x", "Dérivation de conducteurs", "W", [(0, -40), (0, 40)], [L(0, -40, 0, 40), W(3), P(-18, -22, -8, -22, 8, 0), W(2)])

G("Unifilaire - Haute Tension")
def half(id, name, corner):
    pts = {"bl": (-30, -30, -30, 30, 30, 30), "tl": (-30, 30, -30, -30, 30, -30), "tr": (-30, -30, 30, -30, 30, 30), "br": (30, -30, 30, 30, -30, 30)}[corner]
    add(id, name, "Q", [], [W(3), R(-30, -30, 60, 60), W(2), K(*pts)])
half("ht_c1", "Cellule HT - repère bas gauche", "br"); half("ht_c2", "Cellule HT - repère haut gauche", "tl")
half("ht_c3", "Cellule HT - repère haut droit", "tr"); half("ht_c4", "Cellule HT - repère bas droit", "bl")
add("ht_pr", "Prise de courant HT (embrochable)", "X", [(0, -40), (0, 40)], [L(0, -40, 0, 40), P(-10, -14, 0, -4, 10, -14), P(-10, -8, 0, 2, 10, -8)])
add("ht_dj", "Disjoncteur HT", "Q", [(0, -40), (0, 40)], [L(0, -40, 0, -16), L(-3, -19, 3, -13), L(-3, -13, 3, -19), L(0, 26, -14, -14), L(0, 26, 0, 40)])
add("ht_fu", "Fusible HT", "F", [(0, -40), (0, 40)], [L(0, -40, 0, 40), R(-5, -16, 10, 32)])
add("ht_tri", "Départ câble HT", "X", [(0, -30), (0, 30)], [W(4), Q(-28, -22, 28, -22, 0, 26), W(2), L(0, -30, 0, -22), L(0, 26, 0, 30)])
add("ht_ter", "Sectionneur de mise à la terre", "Q", [(-40, 0), (0, 0)], [L(-40, 0, 0, 0), L(-22, 10, 16, -6) if False else L(-40, 10, 12, -8), L(22, -16, 22, 16), L(28, -10, 28, 10), L(34, -5, 34, 5), L(12, 0, 22, 0) if False else L(0, 0, 0, 0)])
add("ht_sec", "Sectionneur HT", "Q", [(0, -40), (0, 40)], [L(0, -40, 0, -16), L(-6, -16, 6, -16), L(0, 26, -14, -14), L(0, 26, 0, 40)])
def tr_uni(id, name, s1, s2):
    add(id, name, "T", [(0, -40), (0, 40)], [L(0, -40, 0, -26), C(0, -12, 14), C(0, 12, 14), L(0, 26, 0, 40), T(0, -12, 12, s1), T(0, 12, 12, s2)])
tr_uni("ht_dy", "Transformateur triangle / étoile", "Δ", "Y")
tr_uni("ht_dyn", "Transformateur triangle / étoile neutre sorti", "Δ", "Yn")
tr_uni("ht_yd", "Transformateur étoile / triangle", "Y", "Δ")

# =================================================================== sortie
def esc(s):
    out = []
    for ch in s:
        if ord(ch) < 128:
            out.append("\\" + ch if ch in '"\\' else ch)
        else:
            out.append("".join("\\%03o" % x for x in ch.encode("utf-8")))
            out.append('""')                 # coupe la sequence octale
    return "".join(out)
seen = set(); rows = []
for e in S:
    if e[0] in seen: raise SystemExit("doublon " + e[0])
    seen.add(e[0])
    rows.append("    {%s}," % ", ".join('"%s"' % esc(x) for x in e))
out = ("// Fichier genere par tools_gen_lib.py - ne pas modifier a la main.\n#pragma once\n#include \"elib_spec.h\"\n"
       "inline const LibSpec kLib[] = {\n" + "\n".join(rows) + "\n};\ninline const int kLibCount = %d;\n" % len(S))
open(sys.argv[1], "w", encoding="utf-8").write(out)
groups = []
for e in S:
    if e[2] not in groups: groups.append(e[2])
print(len(S), "symboles,", len(groups), "dossiers")
if len(sys.argv) > 2:
    import json; json.dump({g: [e[0] for e in S if e[2] == g] for g in groups}, open(sys.argv[2], "w"))
