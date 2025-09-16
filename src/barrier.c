#include "barrier.h"

int otp_barrier_init(otp_barrier_t *barrier, unsigned count) {
	if (!barrier || count == 0) return -1;
	barrier->count_total = (int)count;
	barrier->count_waiting = 0;
	if (pthread_mutex_init(&barrier->mutex, NULL) != 0) return -1;
	if (pthread_cond_init(&barrier->cond, NULL) != 0) {
		pthread_mutex_destroy(&barrier->mutex);
		return -1;
	}
	return 0;
}

int otp_barrier_wait(otp_barrier_t *barrier) {
	if (!barrier) return -1;
	if (pthread_mutex_lock(&barrier->mutex) != 0) return -1;
	barrier->count_waiting++;
	if (barrier->count_waiting >= barrier->count_total) {
		barrier->count_waiting = 0;
		pthread_cond_broadcast(&barrier->cond);
		pthread_mutex_unlock(&barrier->mutex);
		return 1; 
	} else {
		while (barrier->count_waiting != 0) {
			pthread_cond_wait(&barrier->cond, &barrier->mutex);
		}
		pthread_mutex_unlock(&barrier->mutex);
		return 0;
	}
}

int otp_barrier_destroy(otp_barrier_t *barrier) {
	if (!barrier) return -1;
	pthread_cond_destroy(&barrier->cond);
	pthread_mutex_destroy(&barrier->mutex);
	return 0;
}


