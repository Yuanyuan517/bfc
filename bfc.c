#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include "bfc.h"

#define BFC_VERSION "r83"

int bfc_verbose = 3;
double bfc_real_time;
bfc_kmer_t bfc_kmer_null = {{0,0,0,0}};

void bfc_opt_init(bfc_opt_t *opt)
{
	memset(opt, 0, sizeof(bfc_opt_t));
	opt->chunk_size = 100000000;
	opt->n_threads = 1;
	opt->k = 33;
	opt->q = 20;
	opt->n_shift = 33;
	opt->n_hashes = 4;

	opt->min_cov = 3;
	opt->win_multi_ec = 10;
	opt->max_end_ext = 5;

	opt->w_ec = 1;
	opt->w_ec_high = 7;
	opt->w_absent = 3;
	opt->w_absent_high = 1;
	opt->max_path_diff = 15;
}

void bfc_opt_by_size(bfc_opt_t *opt, long size)
{
	opt->k = (int)(log(size) / log(2) * 1.2);
	if (opt->k > 37) opt->k = 37;
	opt->n_shift = opt->k;
}

static void usage(FILE *fp, bfc_opt_t *o)
{
	fprintf(fp, "Usage: bfc [options] <in.fq>\n");
	fprintf(fp, "Options:\n");
	fprintf(fp, "  -s FLOAT     approx genome size (k/m/g allowed; change -k and -b) [unset]\n");
	fprintf(fp, "  -k INT       k-mer length [%d]\n", o->k);
	fprintf(fp, "  -t INT       number of threads [%d]\n", o->n_threads);
	fprintf(fp, "  -b INT       set Bloom filter size to pow(2,INT) bits [%d]\n", o->n_shift);
	fprintf(fp, "  -H INT       use INT hash functions for Bloom filter [%d]\n", o->n_hashes);
	fprintf(fp, "  -d FILE      dump hash table to FILE [null]\n");
	fprintf(fp, "  -E           skip error correction\n");
	fprintf(fp, "  -r FILE      restore hash table from FILE [null]\n");
	fprintf(fp, "  -w INT       no more than %d ec or 2 highQ ec in INT-bp window [%d]\n", BFC_EC_HIST, o->win_multi_ec);
	fprintf(fp, "  -c INT       min k-mer coverage [%d]\n", o->min_cov);
	fprintf(fp, "  -D           discard uncorrectable reads\n");
	fprintf(fp, "  -v           show version number\n");
	fprintf(fp, "  -h           show command line help\n");
}

int main(int argc, char *argv[])
{
	bfc_opt_t opt;
	bfc_bf_t *bf = 0;
	bfc_ch_t *ch = 0;
	int i, c, no_ec = 0;
	char *in_hash = 0, *out_hash = 0;

	bfc_real_time = realtime();
	bfc_opt_init(&opt);
	while ((c = getopt(argc, argv, "hvV:Ed:k:s:b:L:t:C:H:q:Jr:c:w:D1")) >= 0) {
		if (c == 'k') opt.k = atoi(optarg);
		else if (c == 'd') out_hash = optarg;
		else if (c == 'r') in_hash = optarg;
		else if (c == 'q') opt.q = atoi(optarg);
		else if (c == 'b') opt.n_shift = atoi(optarg);
		else if (c == 't') opt.n_threads = atoi(optarg);
		else if (c == 'H') opt.n_hashes = atoi(optarg);
		else if (c == 'c') opt.min_cov = atoi(optarg);
		else if (c == 'w') opt.win_multi_ec = atoi(optarg);
		else if (c == 'D') opt.discard = 1;
		else if (c == '1') opt.filter_mode = 1;
		else if (c == 'J') opt.no_mt_io = 1; // for debugging kt_pipeline()
		else if (c == 'E') no_ec = 1;
		else if (c == 'V') bfc_verbose = atoi(optarg);
		else if (c == 'h') {
			usage(stdout, &opt);
			return 0;
		} else if (c == 'v') {
			printf("%s\n", BFC_VERSION);
			return 0;
		} else if (c == 'L' || c == 's') {
			double x;
			char *p;
			x = strtod(optarg, &p);
			if (*p == 'G' || *p == 'g') x *= 1e9;
			else if (*p == 'M' || *p == 'm') x *= 1e6;
			else if (*p == 'K' || *p == 'k') x *= 1e3;
			if (c == 's') {
				bfc_opt_by_size(&opt, (long)x + 1);
				fprintf(stderr, "[M::%s] set k to %d\n", __func__, opt.k);
			} else if (c == 'L') opt.chunk_size = (long)x + 1;
		}
	}

	if (optind == argc) {
		usage(stderr, &opt);
		return 1;
	}

	if (opt.filter_mode) bf = (bfc_bf_t*)bfc_count(argv[optind], &opt);
	else if (in_hash) ch = bfc_ch_restore(in_hash);
	else ch = (bfc_ch_t*)bfc_count(argv[optind], &opt);

	if (ch) {
		if (out_hash) bfc_ch_dump(ch, out_hash);
		if (!no_ec) bfc_correct(argv[optind], &opt, ch);
		bfc_ch_destroy(ch);
	} else if (bf) {
		bfc_bf_destroy(bf);
	}

	fprintf(stderr, "[M::%s] Version: %s\n", __func__, BFC_VERSION);
	fprintf(stderr, "[M::%s] CMD:", __func__);
	for (i = 0; i < argc; ++i)
		fprintf(stderr, " %s", argv[i]);
	fprintf(stderr, "\n[M::%s] Real time: %.3f sec; CPU: %.3f sec\n", __func__, realtime() - bfc_real_time, cputime());
	return 0;
}
