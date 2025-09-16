#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#include "barrier.h"
#include "lcg.h"

typedef struct worker_ctx_s {
	const uint8_t *plaintext;
	const uint8_t *keystream;
	uint8_t *ciphertext;
	size_t offset;
	size_t length;
	otp_barrier_t *barrier;
} worker_ctx_t;

static void die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputs("\n", stderr);
	exit(EXIT_FAILURE);
}

static void *worker_main(void *arg) {
	worker_ctx_t *ctx = (worker_ctx_t *)arg;
	const uint8_t *p = ctx->plaintext + ctx->offset;
	const uint8_t *k = ctx->keystream + ctx->offset;
	uint8_t *c = ctx->ciphertext + ctx->offset;
	for (size_t i = 0; i < ctx->length; ++i) {
		c[i] = p[i] ^ k[i];
	}
	otp_barrier_wait(ctx->barrier);
	return NULL;
}

static long get_cpu_cores(void) {

#if defined(__APPLE__)
	int mib[2] = { CTL_HW, HW_AVAILCPU };
	int ncpu = 0;
	size_t len = sizeof(ncpu);
	if (sysctl(mib, 2, &ncpu, &len, NULL, 0) != 0 || ncpu < 1) {
		mib[1] = HW_NCPU;
		len = sizeof(ncpu);
		if (sysctl(mib, 2, &ncpu, &len, NULL, 0) != 0 || ncpu < 1) {
			ncpu = 1;
		}
	}
	return (long)ncpu;
#else
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	if (n < 1) n = 1;
	return n;
#endif
}

static void usage(const char *prog) {
	fprintf(stderr,
			"Usage: %s -i <input> -o <output> -x <seed> -a <a> -c <c> -m <m>\n",
			prog);
	fprintf(stderr,
			"Example: %s -i plain.txt -o cipher.bin -x 4212 -a 84589 -c 45989 -m 217728\n",
			prog);
}

int main(int argc, char **argv) {
	const char *in_path = NULL;
	const char *out_path = NULL;
	lcg_params_t params = {0};
	size_t max_size = 1024ull * 1024ull * 1024ull;

	int opt;
	while ((opt = getopt(argc, argv, "i:o:x:a:c:m:h")) != -1) {
		switch (opt) {
			case 'i': in_path = optarg; break;
			case 'o': out_path = optarg; break;
			case 'x': params.seed = strtoull(optarg, NULL, 10); break;
			case 'a': params.a = strtoull(optarg, NULL, 10); break;
			case 'c': params.c = strtoull(optarg, NULL, 10); break;
			case 'm': params.m = strtoull(optarg, NULL, 10); break;
			case 'h': default: usage(argv[0]); return (opt=='h')?0:1;
		}
	}

	if (!in_path || !out_path || params.m == 0) {
		usage(argv[0]);
		return 1;
	}

	int fd_in = open(in_path, O_RDONLY);
	if (fd_in < 0) die("open input failed: %s", strerror(errno));
	struct stat st;
	if (fstat(fd_in, &st) != 0) die("fstat failed: %s", strerror(errno));
	size_t file_size = (size_t)st.st_size;
	if (file_size == 0) die("input file is empty");
	if (file_size > max_size) die("input too large (limit %zu bytes)", max_size);

	uint8_t *plain = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd_in, 0);
	if (plain == MAP_FAILED) die("mmap input failed: %s", strerror(errno));
	close(fd_in);

	int fd_out = open(out_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd_out < 0) die("open output failed: %s", strerror(errno));
	if (ftruncate(fd_out, (off_t)file_size) != 0) die("ftruncate failed: %s", strerror(errno));
	uint8_t *cipher = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_out, 0);
	if (cipher == MAP_FAILED) die("mmap output failed: %s", strerror(errno));
	close(fd_out);

	uint8_t *keystream = malloc(file_size);
	if (!keystream) die("malloc keystream failed");

	lcg_job_t job = {
		.params = params,
		.output = { .buffer = keystream, .size = file_size }
	};
	pthread_t lcg_thread;
	if (pthread_create(&lcg_thread, NULL, lcg_thread_main, &job) != 0) {
		die("pthread_create lcg failed");
	}
	pthread_join(lcg_thread, NULL);

	long core_count = get_cpu_cores();
	if (core_count < 1) core_count = 1;
	if ((size_t)core_count > file_size) core_count = (long)file_size; // cap by bytes

	otp_barrier_t barrier;
	if (otp_barrier_init(&barrier, (unsigned)(core_count + 1)) != 0) die("barrier init failed");

	pthread_t *threads = calloc((size_t)core_count, sizeof(pthread_t));
	worker_ctx_t *ctxs = calloc((size_t)core_count, sizeof(worker_ctx_t));
	if (!threads || !ctxs) die("alloc threads failed");

	size_t base = file_size / (size_t)core_count;
	size_t rem = file_size % (size_t)core_count;
	size_t offset = 0;
	for (long i = 0; i < core_count; ++i) {
		size_t len = base + (i == core_count - 1 ? rem : 0);
		ctxs[i].plaintext = plain;
		ctxs[i].keystream = keystream;
		ctxs[i].ciphertext = cipher;
		ctxs[i].offset = offset;
		ctxs[i].length = len;
		ctxs[i].barrier = &barrier;
		if (pthread_create(&threads[i], NULL, worker_main, &ctxs[i]) != 0) die("pthread_create worker failed");
		offset += len;
	}

	otp_barrier_wait(&barrier);

	for (long i = 0; i < core_count; ++i) pthread_join(threads[i], NULL);
	otp_barrier_destroy(&barrier);

	msync(cipher, file_size, MS_SYNC);
	munmap(plain, file_size);
	munmap(cipher, file_size);
	free(keystream);
	free(threads);
	free(ctxs);
	return 0;
}


