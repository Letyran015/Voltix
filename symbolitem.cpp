#include "symbolitem.h"
#include "wireitem.h"

static const qreal GRID_STEP = 20;

SymbolItem::SymbolItem()
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
}

SymbolItem::~SymbolItem()
{
    const auto ws = m_wires;
    for (WireItem *w : ws)
        w->detachSymbol(this);   // les fils restent, mais deviennent libres
}

void SymbolItem::updateWires()
{
    for (WireItem *w : std::as_const(m_wires))
        w->syncEnds();
}

QVariant SymbolItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {          // accroche à la grille
        const QPointF p = value.toPointF();
        return QPointF(qRound(p.x() / GRID_STEP) * GRID_STEP,
                       qRound(p.y() / GRID_STEP) * GRID_STEP);
    }
    if (change == ItemPositionHasChanged || change == ItemTransformHasChanged
        || change == ItemRotationHasChanged)
        updateWires();                                      // les fils suivent
    return QGraphicsItem::itemChange(change, value);
}