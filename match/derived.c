#include "engine.h"

/* Two api functions you were not asked to implement
   returns the best price at the bid or ask */
t_price bestbid();
t_price bestask();
 
/* A market order which executes immediately at the best
   price possible
   IN: order: market order to add to book, price ignored
   OUT: order id of new order */
t_orderid market(t_order order) {
  order.price = is_ask(order.side) ? MAX_PRICE : MIN_PRICE;
  return limit(order);
}

/* Atomically replace an order on the book
   used by high frequency traders to ensure 
   that the new order and the old order cannot 
   both get executed
   IN: orderid: id of order to replace
   OUT: order id of new order */
t_orderid replace(t_orderid orderid, t_order order) {
  cancel(orderid); 
  return limit(order);
}

/* An order type that is guaranteed to add liquidity
   used by market makes and rebate arbitrageurs
   IN: order: price will be ignored
       offset: number of ticks from side of NBBO
   OUT: orderid assigned to order */
t_orderid post(t_order order, t_price offset) {
  order.price = (is_ask(order.side) ? 
		 bestbid() - offset :
		 bestask() + offset);
  return limit(order);
}

