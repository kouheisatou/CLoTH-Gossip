#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_math.h>
#include "../include/array.h"
#include "../include/htlc.h"
#include "../include/payments.h"
#include "../include/network.h"

#define MAXMICRO 1E3
#define MINMICRO 1
#define MAXSMALL 1E8
#define MINSMALL 1E3
#define MAXMEDIUM 1E11
#define MINMEDIUM 1E8


struct payment* new_payment(long id, long sender, long receiver, uint64_t amount, uint64_t start_time) {
  struct payment * p;

  p = malloc(sizeof(struct payment));
  p->id=id;
  p->sender= sender;
  p->receiver = receiver;
  p->amount = amount;
  p->start_time = start_time;
  p->route = NULL;
  p->is_success = 0;
  p->offline_node_count = 0;
  p->no_balance_count = 0;
  p->is_timeout = 0;
  p->end_time = 0;
  p->attempts = 0;
  p->error.type = NOERROR;
  p->error.hop = NULL;

  return p;
}



void initialize_random_payments(struct payments_params pay_params, long n_nodes, gsl_rng * random_generator) {
  long i, sender_id, receiver_id;
  uint64_t  payment_amount=0, payment_time=1, next_payment_interval ;
  long payment_idIndex=0;
  int base, exp;
  int npay[8]={0};
  FILE* payments_file;

  payments_file = fopen("payments.csv", "w");
  if(payments_file==NULL) {
    fprintf(stderr, "ERROR: cannot open file payments.csv\n");
    exit(-1);
  }
  fprintf(payments_file, "id,sender,receiver,amount,time\n");


  for(i=0;i<pay_params.n_payments;i++) {

    do{
      sender_id = gsl_rng_uniform_int(random_generator,n_nodes);
      receiver_id = gsl_rng_uniform_int(random_generator, n_nodes);
    } while(sender_id==receiver_id);



    do{
      exp = gsl_ran_gaussian_tail(random_generator, 3, pay_params.sigma_amount);
    } while(exp>8);

    ++npay[exp-3];

    base = gsl_rng_uniform_int(random_generator, 8)+1;

    payment_amount = base*gsl_pow_int(10,exp);

    next_payment_interval = 1000*gsl_ran_exponential(random_generator, pay_params.payment_mean);
    payment_time += next_payment_interval;

    fprintf(payments_file, "%ld,%ld,%ld,%ld,%ld\n", payment_idIndex++, sender_id, receiver_id, payment_amount, payment_time );

  }

  /* for(i=0; i<8; i++) */
  /*   printf("%d, %d\n", i+3, npay[i]); */

  //  exit(-1);

  fclose(payments_file);
}


struct array* generate_payments(struct payments_params pay_params) {
  struct payment* payment;
  char row[256], payments_filename[256];
  long id, sender, receiver;
  uint64_t amount, time;
  struct array* payments;
  FILE* payments_file;

  if(!(pay_params.payments_from_file))
    strcpy(payments_filename, "payments.csv");
  else
    strcpy(payments_filename, pay_params.payments_filename);

  payments_file = fopen(payments_filename, "r");
  if(payments_file==NULL) {
    printf("ERROR: cannot open file <%s>\n", payments_filename);
    exit(-1);
  }

  payments = array_initialize(1000);

  fgets(row, 256, payments_file);
  while(fgets(row, 256, payments_file) != NULL) {
    sscanf(row, "%ld,%ld,%ld,%"SCNu64",%"SCNu64"", &id, &sender, &receiver, &amount, &time);
    payment = new_payment(id, sender, receiver, amount, time);
    payments = array_insert(payments, payment);
  }
  fclose(payments_file);

  return payments;
}


struct array* initialize_payments(struct payments_params pay_params, long n_nodes, gsl_rng* random_generator) {
  if(!(pay_params.payments_from_file))
    initialize_random_payments(pay_params, n_nodes, random_generator);

  return generate_payments(pay_params);
}
