#ifndef OTP_LCG_H
#define OTP_LCG_H

#include <stddef.h>
#include <stdint.h>

typedef struct lcg_params_s {
	uint64_t seed;
	uint64_t a;
	uint64_t c;
	uint64_t m; 
} lcg_params_t;

typedef struct lcg_output_s {
	uint8_t *buffer;
	size_t size;
} lcg_output_t;

typedef struct lcg_job_s {
	lcg_params_t params;
	lcg_output_t output;
}	lcg_job_t;

void *lcg_thread_main(void *arg);

#endif 


