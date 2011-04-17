#include "engine.h"

/* IN: order: market order to add to book, price ignored
   OUT: order id of new order */
t_orderid market(t_order order);

/* IN: orderid: id of order to replace
   OUT: order id of new order */
t_orderid replace(t_orderid orderid, t_order order);

/* IN: order: price will be ignored
       offset: number of ticks from side of NBBO
   OUT: orderid assigned to order */
t_orderid post(t_order order, t_price offset);

