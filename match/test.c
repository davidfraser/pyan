#include <stdio.h>
#include "limits.h"
#include "types.h"
#include "engine.c"

/* Besides crashing, the only way to sense 
   what is happening internally via the 
   required API functions is to submit 
   combinations of orders that trigger
   execution notifications. Normally the 
   tests would scale up more smoothly
   but because of the api's opacity, 
   the tests require implementation of 
   multiple bits of base functionality
   in limit, cancel, and execution to even 
   get to the most basic nontrivial tests. */

#define TEST(num, orders, execs) t_order o ## num [] = orders ; t_execution x ## num [] = execs ; correct += test( o ## num , sizeof( o ## num )/sizeof(t_order) , x ## num , sizeof( x ## num )/sizeof(t_execution));
#define TEST_CANCEL(num, orders1, cancels, orders2, execs) t_order o1st ## num [] = orders1 ; t_orderid c ## num [] = cancels ; t_order o2nd ## num [] = orders2 ; t_execution x ## num [] = execs ; correct += test_cancel( o1st ## num , sizeof( o1st ## num )/sizeof(t_order), c ## num , sizeof( c ## num )/sizeof(t_orderid), o2nd ## num , sizeof( o2nd ## num )/sizeof(t_order) , x ## num, sizeof( x ## num )/sizeof(t_execution));
#define X ,
#define MAX_EXECS 100
unsigned correct = 0;
t_orderid orderid;
unsigned totaltests = 0;
t_execution execs_out[MAX_EXECS];
t_execution * execs_out_iter;
unsigned execs_out_len;
int exec_overflow;

t_order oa101x100 = {"JPM", "MAX", 1, 101, 100};
t_order ob101x100 = {"JPM", "MAX", 0, 101, 100};
t_order oa101x50 =  {"JPM", "MAX", 1, 101, 50};
t_order ob101x50 =  {"JPM", "MAX", 0, 101, 50};
t_order oa101x25 =  {"JPM", "MAX", 1, 101, 25};
t_order ob101x25 =  {"JPM", "MAX", 0, 101, 25};
t_order ob101x25x = {"JPM", "XAM", 0, 101, 25};

t_execution xa101x100 = {"JPM", "MAX", 1, 101, 100};
t_execution xb101x100 = {"JPM", "MAX", 0, 101, 100};
t_execution xa101x50 =  {"JPM", "MAX", 1, 101, 50};
t_execution xb101x50 =  {"JPM", "MAX", 0, 101, 50};
t_execution xa101x25 =  {"JPM", "MAX", 1, 101, 25};
t_execution xb101x25 =  {"JPM", "MAX", 0, 101, 25};
t_execution xb101x25x = {"JPM", "XAM", 0, 101, 25};

int main() {
  printf("ECN Matching Engine Autotester Running\n"
	 "--------------------------------------\n");  

  // ask
  TEST(1, {oa101x100}, {}); 
  // bid
  TEST(2, {ob101x100}, {}); 

  // execution
  TEST(3, {oa101x100 X ob101x100}, {xa101x100 X xb101x100}); 

  // reordering
  TEST(4, {oa101x100 X ob101x100}, {xb101x100 X xa101x100}); 
  TEST(5, {ob101x100 X oa101x100}, {xa101x100 X xb101x100}); 
  TEST(6, {ob101x100 X oa101x100}, {xb101x100 X xa101x100}); 

  // partial fill
  TEST(7, {oa101x100 X ob101x50}, {xa101x50 X xb101x50}); 
  TEST(8, {oa101x50 X ob101x100}, {xa101x50 X xb101x50}); 
  
  // incremental over fill 
  TEST(9, {oa101x100 X ob101x25 X ob101x25 X ob101x25 X ob101x25 X ob101x25}, {xa101x25 X xb101x25 X xa101x25 X xb101x25 X xa101x25 X xb101x25 X xa101x25 X xb101x25});
  TEST(10, {ob101x100 X oa101x25 X oa101x25 X oa101x25 X oa101x25 X oa101x25}, {xa101x25 X xb101x25 X xa101x25 X xb101x25 X xa101x25 X xb101x25 X xa101x25 X xb101x25});

  // queue position
  TEST(11, {ob101x25x X ob101x25 X oa101x25}, {xa101x25 X xb101x25x}); 

  // cancel so no execution
  TEST_CANCEL(12, {ob101x25}, {1}, {oa101x25}, {});

  // cancel from front of queue
  TEST_CANCEL(13, {ob101x25x X ob101x25}, {1}, {oa101x25}, {xa101x25 X xb101x25});

  // cancel front, back, out of order then partial execution
  TEST_CANCEL(14, {ob101x100 X ob101x25x X ob101x25x X ob101x50}, {1 X 4 X 3}, {oa101x50}, {xb101x25x X xa101x25});

  printf("--------------------------------------\n");  
  printf("You got %i/%i tests correct.\n", correct, totaltests);
}

void execution(t_execution exec) {
  execs_out_len++;
  if (exec_overflow || (execs_out_iter == &execs_out[MAX_EXECS])) {
    exec_overflow = 1;
  }
  *execs_out_iter = exec;
  execs_out_iter++;
}

void set_globals() {
  orderid = 0;
  totaltests++;
  exec_overflow = 0;
  execs_out_iter = execs_out;
  execs_out_len = 0;
}

int feed_orders(t_order orders[], unsigned orders_len) {
  int id;
  unsigned i;
  for(i = 0; i < orders_len; i++) {
    id = limit(orders[i]);
    orderid++;
    if (id != orderid) {
      printf("orderid returned was %u, should have been %u.\n", 
	     id, i+1);
      return 0;
    }
  }
  return 1;
}

int feed_cancels(t_orderid cancels[], unsigned cancels_len) {
  unsigned i;
  for(i = 0; i < cancels_len; i++) {
    cancel(cancels[i]); 
  }
  return 1;
}


int assert_exec_count(unsigned num_execs_expected) {
  if (exec_overflow) {
    printf("too many executions, test array overflow");
    return 0;
  }
  int correct = execs_out_len == num_execs_expected;
  if (!correct) {
    printf("execution called %u times, should have been %u.\n",
	   execs_out_len, num_execs_expected);
  }
  return correct;
}

int exec_eq(t_execution * e1, t_execution * e2) {
  int eq = 1; 
  int i;
  for (i = 0; i < STRINGLEN; i++) {
    if (e1->symbol[i] == '\0' && e2->symbol[i] == '\0') break;
    eq = eq && 
      e1->symbol[i] == e2->symbol[i] && 
      e1->trader[i] == e2->trader[i];
  }
  eq = eq && 
    e1->side == e2->side && 
    e1->price == e2->price && 
    e1->size == e2->size;
  return eq;
}

int assert_execs(t_execution execs[], unsigned execs_len) {
  unsigned i;
  for(i = 0; i < execs_len; i+=2) {
    if(!((exec_eq(&execs[i], &execs_out[i]) && 
	  exec_eq(&execs[i+1], &execs_out[i+1])) || 
	 (exec_eq(&execs[i], &execs_out[i+1]) && 
	  exec_eq(&execs[i+1], &execs_out[i]))))  {
      printf("executions #%u and #%u,\n"
	     "{symbol=%s, trader=%s, side=%i, price=%u, size=%u},\n"
	     "{symbol=%s, trader=%s, side=%i, price=%u, size=%u}\n"
	     "should have been\n"
	     "{symbol=%s, trader=%s, side=%i, price=%u, size=%u},\n"
	     "{symbol=%s, trader=%s, side=%i, price=%u, size=%u}.\n"
	     "Stopped there.\n", 
	     i, i+1,
	     execs_out[i].symbol, execs_out[i].trader, execs_out[i].side, execs_out[i].price, (unsigned)execs_out[i].size,
	     execs_out[i+1].symbol, execs_out[i+1].trader, execs_out[i+1].side, execs_out[i+1].price, (unsigned)execs_out[i+1].size,
	     execs[i].symbol, execs[i].trader, execs[i].side, execs[i].price, (unsigned)execs[i].size,
	     execs[i+1].symbol, execs[i+1].trader, execs[i+1].side, execs[i+1].price, (unsigned)execs[i+1].size);
      return 0;
    }
  }
  return 1;
}

/* IN: orders: sequence of orders
   OUT: points received on test */
int test(t_order orders[], unsigned orders_len, t_execution execs[], unsigned execs_len) {
  int ok = 1;
  set_globals();
  init();
  ok = ok && feed_orders(orders, orders_len);
  ok = ok && assert_exec_count(execs_len);
  ok = ok && assert_execs(execs, execs_len);
  destroy();
  if (!ok) printf("test %i failed.\n\n", totaltests);
  return ok;
}

/* IN: orders: sequence of orders
   OUT: points received on test */
int test_cancel(t_order orders1[], unsigned orders1_len, t_orderid cancels[], unsigned cancels_len, t_order orders2[], unsigned orders2_len, t_execution execs[], unsigned execs_len) {
  int ok = 1;
  set_globals();
  init();
  ok = ok && feed_orders(orders1, orders1_len);
  feed_cancels(cancels, cancels_len);
  ok = ok && feed_orders(orders2, orders2_len);
  ok = ok && assert_exec_count(execs_len);
  ok = ok && assert_execs(execs, execs_len);
  destroy();
  if (!ok) printf("test %i failed.\n\n", totaltests);
  return ok;
}

