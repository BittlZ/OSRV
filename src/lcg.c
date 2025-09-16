#include "lcg.h"

#include <stddef.h>

static inline uint64_t lcg_next(uint64_t x, const lcg_params_t *p) {
	return (p->a * x + p->c) % p->m;
}

void *lcg_thread_main(void *arg) {
	lcg_job_t *job = (lcg_job_t *)arg;
	if (!job || !job->output.buffer || job->output.size == 0) return NULL;

	uint64_t x = job->params.seed;
	uint8_t *out = job->output.buffer;
	size_t n = job->output.size;

	for (size_t i = 0; i < n; ++i) {
		x = lcg_next(x, &job->params);
		out[i] = (uint8_t)(x & 0xFFu);
	}

	return NULL;
}


