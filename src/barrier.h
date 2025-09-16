#ifndef OTP_BARRIER_H
#define OTP_BARRIER_H

#include <pthread.h>

typedef struct otp_barrier_s {
	int count_total;
	int count_waiting;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} otp_barrier_t;

int otp_barrier_init(otp_barrier_t *barrier, unsigned count);
int otp_barrier_wait(otp_barrier_t *barrier);
int otp_barrier_destroy(otp_barrier_t *barrier);

#endif 


