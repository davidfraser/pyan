/*
  Most basic implementation of the matching engine

  O(n) add
  O(n) cancel
*/

//#include <stdio.h>
#include "engine.h"

// INTERNAL TYPES
/* Limit Order Internal */ 
typedef struct {
  t_order order;
  t_orderid id;
} t_order_in;

// DOUBLY LINKED LIST
#define MYDATA t_order_in
#define MYDATAINIT {{'', '', 0, 0, 0}, 0}
#include "double_link_list.c"

// GLOBALS
/* Stores all live bids and asks*/
list * bids;
list * asks;
/* The next ID which will be assigned an order */
t_orderid nextid;

// IMPLEMENTATION HELPERS
/* Returns the node with the given id in
   list l, or NULL if no node has that id */
node * find_node(t_orderid id, list * l) {
  node * node_iter = l->head->next;
  while (node_iter->data.id != id && 
	 node_iter != l->tail) {
    node_iter = node_iter->next;
  }
  if (node_iter == l->tail) { return NULL; }
  else { return node_iter; }
}

/* Call the execution report callback to
   notify both parties to the trade 
   o1 and o2 are assumed to be the same price
   on opposite sides */
void send_exec(t_order * o1, t_order * o2) {
  t_execution exec = (t_execution)(*o1);
  exec.size = o1->size > o2->size ? o2->size : o1->size;
  execution(exec);
  // rename trader field on exec to old's name
  int i;
  for(i = 0; i < STRINGLEN; i++) {
    exec.trader[i] = o2->trader[i]; 
  } 
  exec.side = !exec.side;
  execution(exec);  
}

/* IN: order_new: new order being added to the book
       node_old: old order sitting on the book
       l: side of the book old sits on
   OUT: */
void trade(t_order * order_new, 
	   node * node_old, 
	   list * l) {
  // shorthand
  t_order * order_old = &node_old->data.order;
  // execution report
  send_exec(order_new, order_old);
  // new completely fills old
  if (order_new->size >= order_old->size) {
    order_new->size -= order_old->size;
    remove_node(l, node_old);
  } 
  // new partially fills old
  else {    
    order_old->size -= order_new->size;
    order_new->size = 0;
  }
}

/* helpers for cross function */
int hit_ask(t_price bid, t_price ask) { return bid >= ask; }
int hit_bid(t_price ask, t_price bid) { return ask <= bid; }

// cross as many shares as possible
int cross(t_order * order) {

  // which side
  int isask = is_ask(order->side);
  list * book = isask ? bids : asks;
  int (*cross_test)(t_price, t_price) = 
    isask ? hit_bid : hit_ask;  

  // trade through existing orders one-by-one
  node * book_iter = book->head->next;
  while(book_iter != book->tail &&
	cross_test(order->price, 
		   book_iter->data.order.price)) {
    trade(order, book_iter, book);
    if (order->size == 0) { 
      return 1; 
    }
    book_iter = book_iter->next;
  }
  return 0; 
}

/* helpers for queue function */
int priority_ask(t_price ask_new, t_price ask_old) { return ask_new < ask_old; }
int priority_bid(t_price bid_new, t_price bid_old) { return bid_new > bid_old; }

void queue(t_order * order) {
  int isask = is_ask(order->side);
  list * book = isask ? asks : bids;
  int (*priority_test)(t_price, t_price) = 
    isask ? priority_ask : priority_bid;

  node * book_iter = book->head->next;
  while(book_iter != book->tail &&
	!priority_test(order->price, 
		       book_iter->data.order.price)) {    
    book_iter = book_iter->next; 
  }
  t_order_in o;
  o.order = *order;
  o.id = nextid;
  insert_before(book, book_iter, o);
}

// IMPLEMENTATION
void init() {
  nextid = 1;
  bids = new_list();
  asks = new_list();
}

void destroy() {
  delete_list(bids);
  delete_list(asks);
}

t_orderid limit(t_order order) {
  if (!cross(&order)) queue(&order); 
  return nextid++;
}

void cancel(t_orderid orderid) {
  // look for orderid in bids
  node * node = find_node(orderid, bids);
  if (node) { remove_node(bids, node); }
  else {
    // look for orderid in asks
    node = find_node(orderid, asks);
    if (node) { remove_node(asks, node); }
  }
}

