/* Pi computation using Chudnovsky's algortithm.

 * Copyright 2002, 2005 Hanhong Xue (macroxue at yahoo dot com)

 * Slightly modified 2005 by Torbjorn Granlund (tege at swox dot com) to allow
   more than 2G digits to be computed.

 * Modifed 2008 by David Carver (dcarver at tacc dot utexas dot edu) to enable
   multi-threading using the algorithm from "Computation of High-Precision
   Mathematical Constants in a Combined Cluster and Grid Environment" by
   Daisuke Takahashi, Mitsuhisa Sato, and Taisuke Boku.

   For gcc 4.3
   gcc -fopenmp -Wall -O2 -o pgmp-chudnovsky pgmp-chudnovsky.c -lgmp -lm

   For Intel 10.1 compiler
   icc -openmp  -O2 -o pgmp-chudnovsky pgmp-chudnovsky.c -lgmp -lm

   For AIX xlc
   xlc_r -qsmp=omp -O2 -o pgmp-chudnovsky pgmp-chudnovsky.c  -lgmp -lm

   Note: add -DNO_FACTOR to disable factorization performance enhancement and
   use less memory.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "gmp.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define A 13591409
#define B 545140134
#define C 640320
#define D 12

#define BITS_PER_DIGIT 3.32192809488736234787
#define DIGITS_PER_ITER 14.1816474627254776555
#define DOUBLE_PREC 53

char *prog_name;

#if CHECK_MEMUSAGE
#undef CHECK_MEMUSAGE
#define CHECK_MEMUSAGE                                           \
  do {                                                           \
    char buf[100];                                               \
    snprintf(buf, 100, "ps aguxw | grep '[%c]%s'", prog_name[0], \
             prog_name + 1);                                     \
    system(buf);                                                 \
  } while (0)
#else
#undef CHECK_MEMUSAGE
#define CHECK_MEMUSAGE
#endif

////////////////////////////////////////////////////////////////////////////

double wall_clock() {
  struct timeval timeval;
  (void)gettimeofday(&timeval, (void *)0);
  return (timeval.tv_sec + (timeval.tv_usec / 1000000.0));
}

void print_mpf(mpf_t num, char *context) {
  fprintf(stderr, "%s=", context);
  mpf_t temp;
  mpf_init(temp);
  mpf_set(temp, num);
  mpf_out_str(stderr, 10, 50, temp);
  fprintf(stderr, "\n");
}
void print_mpz(mpz_t num, char *context) {
  fprintf(stderr, "%s=", context);
  mpz_t temp;
  mpz_init(temp);
  mpz_set(temp, num);
  mpz_out_str(stderr, 10, temp);
  fprintf(stderr, "\n");
}

////////////////////////////////////////////////////////////////////////////

mpf_t t1, t2;

// r = sqrt(x)
void my_sqrt_ui(mpf_t r, unsigned long x) {
  unsigned long prec, bits, prec0;

  prec0 = mpf_get_prec(r);

  if (prec0 <= DOUBLE_PREC) {
    mpf_set_d(r, sqrt(x));
    return;
  }

  bits = 0;
  for (prec = prec0; prec > DOUBLE_PREC;) {
    int bit = prec & 1;
    prec = (prec + bit) / 2;
    bits = bits * 2 + bit;
  }

  mpf_set_prec_raw(t1, DOUBLE_PREC);
  mpf_set_d(t1, 1 / sqrt(x));

  while (prec < prec0) {
    prec *= 2;
    if (prec < prec0) {
      /* t1 = t1+t1*(1-x*t1*t1)/2; */
      mpf_set_prec_raw(t2, prec);
      mpf_mul(t2, t1, t1);  // half x half -> full
      mpf_mul_ui(t2, t2, x);
      mpf_ui_sub(t2, 1, t2);
      mpf_set_prec_raw(t2, prec / 2);
      mpf_div_2exp(t2, t2, 1);
      mpf_mul(t2, t2, t1);  // half x half -> half
      mpf_set_prec_raw(t1, prec);
      mpf_add(t1, t1, t2);
    } else {
      prec = prec0;
      /* t2=x*t1, t1 = t2+t1*(x-t2*t2)/2; */
      mpf_set_prec_raw(t2, prec / 2);
      mpf_mul_ui(t2, t1, x);
      mpf_mul(r, t2, t2);  // half x half -> full
      mpf_ui_sub(r, x, r);
      mpf_mul(t1, t1, r);  // half x half -> half
      mpf_div_2exp(t1, t1, 1);
      mpf_add(r, t1, t2);
      break;
    }
    prec -= (bits & 1);
    bits /= 2;
  }
}

// r = y/x   WARNING: r cannot be the same as y.
void my_div(mpf_t r, mpf_t y, mpf_t x) {
  unsigned long prec, bits, prec0;

  prec0 = mpf_get_prec(r);

  if (prec0 <= DOUBLE_PREC) {
    mpf_set_d(r, mpf_get_d(y) / mpf_get_d(x));
    return;
  }

  bits = 0;
  for (prec = prec0; prec > DOUBLE_PREC;) {
    int bit = prec & 1;
    prec = (prec + bit) / 2;
    bits = bits * 2 + bit;
  }

  mpf_set_prec_raw(t1, DOUBLE_PREC);
  mpf_ui_div(t1, 1, x);

  while (prec < prec0) {
    prec *= 2;
    if (prec < prec0) {
      /* t1 = t1+t1*(1-x*t1); */
      mpf_set_prec_raw(t2, prec);
      mpf_mul(t2, x, t1);  // full x half -> full
      mpf_ui_sub(t2, 1, t2);
      mpf_set_prec_raw(t2, prec / 2);
      mpf_mul(t2, t2, t1);  // half x half -> half
      mpf_set_prec_raw(t1, prec);
      mpf_add(t1, t1, t2);
    } else {
      prec = prec0;
      /* t2=y*t1, t1 = t2+t1*(y-x*t2); */
      mpf_set_prec_raw(t2, prec / 2);
      mpf_mul(t2, t1, y);  // half x half -> half
      mpf_mul(r, x, t2);   // full x half -> full
      mpf_sub(r, y, r);
      mpf_mul(t1, t1, r);  // half x half -> half
      mpf_add(r, t1, t2);
      break;
    }
    prec -= (bits & 1);
    bits /= 2;
  }
}

////////////////////////////////////////////////////////////////////////////

#ifndef NO_FACTOR

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

typedef struct {
  unsigned long max_facs;
  unsigned long num_facs;
  unsigned long *fac;
  unsigned long *pow;
} fac_t[1];

typedef struct {
  long int fac;
  long int pow;
  long int nxt;
} sieve_t;

fac_t **fpstack, **fgstack;

sieve_t *sieve;
long int sieve_size;
fac_t *ftmp, *fmul;
clock_t gcd_time = 0;

#define INIT_FACS 32

void fac_show(fac_t f) {
  long int i;
  for (i = 0; i < f[0].num_facs; i++)
    if (f[0].pow[i] == 1)
      fprintf(stderr, "%ld ", f[0].fac[i]);
    else
      fprintf(stderr, "%ld^%ld ", f[0].fac[i], f[0].pow[i]);
  fprintf(stderr, "\n");
}

void fac_reset(fac_t f) { f[0].num_facs = 0; }

void fac_init_size(fac_t f, long int s) {
  if (s < INIT_FACS) s = INIT_FACS;

  f[0].fac = malloc(s * sizeof(unsigned long) * 2);
  f[0].pow = f[0].fac + s;
  f[0].max_facs = s;

  fac_reset(f);
}

void fac_init(fac_t f) { fac_init_size(f, INIT_FACS); }

void fac_clear(fac_t f) { free(f[0].fac); }

void fac_resize(fac_t f, long int s) {
  if (f[0].max_facs < s) {
    fac_clear(f);
    fac_init_size(f, s);
  }
}

// f = base^pow
void fac_set_bp(fac_t f, unsigned long base, long int pow) {
  long int i;
  assert(base < sieve_size);
  for (i = 0, base /= 2; base > 0; i++, base = sieve[base].nxt) {
    f[0].fac[i] = sieve[base].fac;
    f[0].pow[i] = sieve[base].pow * pow;
  }
  f[0].num_facs = i;
  assert(i <= f[0].max_facs);
}

// r = f*g
void fac_mul2(fac_t r, fac_t f, fac_t g) {
  long int i, j, k;

  for (i = j = k = 0; i < f[0].num_facs && j < g[0].num_facs; k++) {
    if (f[0].fac[i] == g[0].fac[j]) {
      r[0].fac[k] = f[0].fac[i];
      r[0].pow[k] = f[0].pow[i] + g[0].pow[j];
      i++;
      j++;
    } else if (f[0].fac[i] < g[0].fac[j]) {
      r[0].fac[k] = f[0].fac[i];
      r[0].pow[k] = f[0].pow[i];
      i++;
    } else {
      r[0].fac[k] = g[0].fac[j];
      r[0].pow[k] = g[0].pow[j];
      j++;
    }
  }
  for (; i < f[0].num_facs; i++, k++) {
    r[0].fac[k] = f[0].fac[i];
    r[0].pow[k] = f[0].pow[i];
  }
  for (; j < g[0].num_facs; j++, k++) {
    r[0].fac[k] = g[0].fac[j];
    r[0].pow[k] = g[0].pow[j];
  }
  r[0].num_facs = k;
  assert(k <= r[0].max_facs);
}

// f *= g
void fac_mul(fac_t f, fac_t g, unsigned long index) {
  fac_t tmp;
  fac_resize(fmul[index], f[0].num_facs + g[0].num_facs);
  fac_mul2(fmul[index], f, g);
  tmp[0] = f[0];
  f[0] = fmul[index][0];
  fmul[index][0] = tmp[0];
}

// f *= base^pow
void fac_mul_bp(fac_t f, unsigned long base, unsigned long pow,
                unsigned long index) {
  fac_set_bp(ftmp[index], base, pow);
  fac_mul(f, ftmp[index], index);
}

// remove factors of power 0
void fac_compact(fac_t f) {
  long int i, j;
  for (i = 0, j = 0; i < f[0].num_facs; i++) {
    if (f[0].pow[i] > 0) {
      if (j < i) {
        f[0].fac[j] = f[0].fac[i];
        f[0].pow[j] = f[0].pow[i];
      }
      j++;
    }
  }
  f[0].num_facs = j;
}

// convert factorized form to number
void bs_mul(mpz_t r, long int a, long int b, unsigned long index) {
  long int i, j;
  if (b - a <= 32) {
    mpz_set_ui(r, 1);
    for (i = a; i < b; i++)
      for (j = 0; j < fmul[index][0].pow[i]; j++)
        mpz_mul_ui(r, r, fmul[index][0].fac[i]);
  } else {
    mpz_t r2;
    mpz_init(r2);
    bs_mul(r2, a, (a + b) / 2, index);
    bs_mul(r, (a + b) / 2, b, index);
    mpz_mul(r, r, r2);
    mpz_clear(r2);
  }
}

mpz_t *gcd;

// f /= gcd(f,g), g /= gcd(f,g)
void fac_remove_gcd(mpz_t p, fac_t fp, mpz_t g, fac_t fg, unsigned long index) {
  long int i, j, k, c;
  fac_resize(fmul[index], min(fp->num_facs, fg->num_facs));
  for (i = j = k = 0; i < fp->num_facs && j < fg->num_facs;) {
    if (fp->fac[i] == fg->fac[j]) {
      c = min(fp->pow[i], fg->pow[j]);
      fp->pow[i] -= c;
      fg->pow[j] -= c;
      fmul[index]->fac[k] = fp->fac[i];
      fmul[index]->pow[k] = c;
      i++;
      j++;
      k++;
    } else if (fp->fac[i] < fg->fac[j]) {
      i++;
    } else {
      j++;
    }
  }
  fmul[index]->num_facs = k;
  assert(k <= fmul[index]->max_facs);

  if (fmul[index]->num_facs) {
    bs_mul(gcd[index], 0, fmul[index]->num_facs, index);
    mpz_tdiv_q(p, p, gcd[index]);
    mpz_tdiv_q(g, g, gcd[index]);
    fac_compact(fp);
    fac_compact(fg);
  }
}

void build_sieve(long int n, sieve_t *s) {
  long int m, i, j, k, id2, jd2;

  sieve_size = n;
  m = (long int)sqrt(n);
  memset(s, 0, sizeof(sieve_t) * n / 2);

  s[1 / 2].fac = 1;
  s[1 / 2].pow = 1;

  for (i = 3; i <= n; i += 2) {
    id2 = i >> 1;
    if (s[id2].fac == 0) {
      s[id2].fac = i;
      s[id2].pow = 1;
      if (i <= m) {
        for (j = i * i, k = id2; j <= n; j += i + i, k++) {
          jd2 = j >> 1;
          if (s[jd2].fac == 0) {
            s[jd2].fac = i;
            if (s[k].fac == i) {
              s[jd2].pow = s[k].pow + 1;
              s[jd2].nxt = s[k].nxt;
            } else {
              s[jd2].pow = 1;
              s[jd2].nxt = k;
            }
          }
        }
      }
    }
  }
}

#endif /* NO_FACTOR */

////////////////////////////////////////////////////////////////////////////

int out = 0;
mpz_t **pstack, **qstack, **gstack;
long int cores = 1, depth, cores_depth;
double progress = 0, percent;

// binary splitting
void sum(unsigned long i, unsigned long j, unsigned long gflag) {
  printf("stack[%ld][0] and stack[%ld][0]\n", i, j);
  mpz_mul(pstack[i][0], pstack[i][0], pstack[j][0]);
  mpz_mul(qstack[i][0], qstack[i][0], pstack[j][0]);
  mpz_mul(qstack[j][0], qstack[j][0], gstack[i][0]);
  mpz_add(qstack[i][0], qstack[i][0], qstack[j][0]);
  if (gflag) {
    mpz_mul(gstack[i][0], gstack[i][0], gstack[j][0]);
  }
}
void bs(unsigned long a, unsigned long b, unsigned long gflag,
        unsigned long level, unsigned long index, unsigned long top) {
#ifndef NO_FACTOR
  unsigned long i, mid;
#else
  unsigned long mid;
#endif
  int ccc;

  if (out & 2) {
    fprintf(
        stderr,
        "bs: a = %ld b = %ld gflag = %ld index = %ld level = %ld top = %ld \n",
        a, b, gflag, index, level, top);
    fflush(stderr);
  }

  if ((b > a) && (b - a == 1)) {
    /*
      g(b-1,b) = (6b-5)(2b-1)(6b-1)
      p(b-1,b) = b^3 * C^3 / 24
      q(b-1,b) = (-1)^b*g(b-1,b)*(A+Bb).
    */
    mpz_set_ui(pstack[index][top], b);
    mpz_mul_ui(pstack[index][top], pstack[index][top], b);
    mpz_mul_ui(pstack[index][top], pstack[index][top], b);
    mpz_mul_ui(pstack[index][top], pstack[index][top], (C / 24) * (C / 24));
    mpz_mul_ui(pstack[index][top], pstack[index][top], C * 24);

    mpz_set_ui(gstack[index][top], 2 * b - 1);
    mpz_mul_ui(gstack[index][top], gstack[index][top], 6 * b - 1);
    mpz_mul_ui(gstack[index][top], gstack[index][top], 6 * b - 5);

    mpz_set_ui(qstack[index][top], b);
    mpz_mul_ui(qstack[index][top], qstack[index][top], B);
    mpz_add_ui(qstack[index][top], qstack[index][top], A);
    mpz_mul(qstack[index][top], qstack[index][top], gstack[index][top]);
    if (b % 2) mpz_neg(qstack[index][top], qstack[index][top]);

#ifndef NO_FACTOR
    i = b;
    while ((i & 1) == 0) i >>= 1;
    fac_set_bp(fpstack[index][top], i, 3);  // b^3
    fac_mul_bp(fpstack[index][top], 3 * 5 * 23 * 29, 3, index);
    fpstack[index][top][0].pow[0]--;

    fac_set_bp(fgstack[index][top], 2 * b - 1, 1);         // 2b-1
    fac_mul_bp(fgstack[index][top], 6 * b - 1, 1, index);  // 6b-1
    fac_mul_bp(fgstack[index][top], 6 * b - 5, 1, index);  // 6b-5
#endif                                                     /* NO_FACTOR */

    if (b > (int)(progress)) {
      fprintf(stderr, ".");
      fflush(stderr);
      progress += percent * 2;
    }

  } else {
    /*
      p(a,b) = p(a,m) * p(m,b)
      g(a,b) = g(a,m) * g(m,b)
      q(a,b) = q(a,m) * p(m,b) + q(m,b) * g(a,m)
    */
    mid = a + (b - a) * 0.5224;  // tuning parameter
    bs(a, mid, 1, level + 1, index, top);

    bs(mid, b, gflag, level + 1, index, top + 1);

    ccc = level == 0;

#ifndef NO_FACTOR
    if (ccc) CHECK_MEMUSAGE;
    if (level >= 4) {  // tuning parameter
      clock_t t = clock();
      fac_remove_gcd(pstack[index][top + 1], fpstack[index][top + 1],
                     gstack[index][top], fgstack[index][top], index);
      gcd_time += clock() - t;
    }
#endif /* NO_FACTOR */


    if (ccc) CHECK_MEMUSAGE;
    mpz_mul(pstack[index][top], pstack[index][top], pstack[index][top + 1]);

    if (ccc) CHECK_MEMUSAGE;
    mpz_mul(qstack[index][top], qstack[index][top], pstack[index][top + 1]);

    if (ccc) CHECK_MEMUSAGE;
    mpz_mul(qstack[index][top + 1], qstack[index][top + 1], gstack[index][top]);
    // print_mpz(qstack[index][top], "q_bs_top");

    if (ccc) CHECK_MEMUSAGE;
    mpz_add(qstack[index][top], qstack[index][top], qstack[index][top + 1]);
    // print_mpz(qstack[index][top], "q_bs_bot");



#ifndef NO_FACTOR
    if (ccc) CHECK_MEMUSAGE;
    fac_mul(fpstack[index][top], fpstack[index][top + 1], index);
#endif /* NO_FACTOR */

    if (gflag) {
      mpz_mul(gstack[index][top], gstack[index][top], gstack[index][top + 1]);
#ifndef NO_FACTOR
      fac_mul(fgstack[index][top], fgstack[index][top + 1], index);
#endif /* NO_FACTOR */
    }
  }

#ifndef NO_FACTOR
  if (out & 2) {
    fprintf(stderr, "p(%ld,%ld)=", a, b);
    fac_show(fpstack[index][top]);
    if (gflag) fprintf(stderr, "g(%ld,%ld)=", a, b);
    fac_show(fgstack[index][top]);
  }
#endif /* NO_FACTOR */

}

int main(int argc, char *argv[]) {
  mpf_t pi, qi;
  long int d = 100, terms, i, j, k, cores_size;
  unsigned long psize, qsize, mid;
  clock_t begin, mid0, mid1, mid2, mid3, mid4, end;
  double wbegin, wmid0, wmid1, wend;

  prog_name = argv[0];

  if (argc == 1) {
    fprintf(stderr, "\nSyntax: %s <digits> <option> <cores>\n", prog_name);
    fprintf(stderr, "      <digits> digits of pi to output\n");
    fprintf(stderr, "      <option> 0 - just run (default)\n");
    fprintf(stderr, "               1 - output digits\n");
    fprintf(stderr, "               2 - debug\n");
    fprintf(stderr, "      <cores> number of cores (default 1)\n");
    exit(1);
  }
  if (argc > 1) d = strtoul(argv[1], 0, 0);
  if (argc > 2) out = atoi(argv[2]);
  if (argc > 3) cores = atoi(argv[3]);

  terms = d / DIGITS_PER_ITER;
  depth = 0;
  while ((1L << depth) < terms) depth++;
  depth++;

  if (cores < 1) {
    fprintf(stderr, "Number of cores reset from %ld to 1\n", cores);
    fflush(stderr);
    cores = 1;
  }
  if ((terms > 0) && (terms < cores)) {
    fprintf(stderr, "Number of cores reset from %ld to %ld\n", cores, terms);
    fflush(stderr);
    cores = terms;
  }
  cores_depth = 0;
  while ((1L << cores_depth) < cores) cores_depth++;
  cores_size = pow(2, cores_depth);

  percent = terms / 100.0;

  fprintf(stderr, "#terms=%ld, depth=%ld, cores=%ld\n", terms, depth, cores);

  begin = clock();
  wbegin = wall_clock();

#ifndef NO_FACTOR
  sieve_size = max(3 * 5 * 23 * 29 + 1, terms * 6);
  sieve = (sieve_t *)malloc(sizeof(sieve_t) * sieve_size / 2);
  build_sieve(sieve_size, sieve);
#endif /* NO_FACTOR */

  mid0 = clock();
  wmid0 = wall_clock();

#ifndef NO_FACTOR
  fprintf(stderr, "sieve   cputime = %6.3f\n",
          (double)(mid0 - begin) / CLOCKS_PER_SEC);
#endif /* NO_FACTOR */

  /* allocate stacks */
  pstack = malloc(sizeof(mpz_t) * cores);
  qstack = malloc(sizeof(mpz_t) * cores);
  gstack = malloc(sizeof(mpz_t) * cores);
  for (j = 0; j < cores; j++) {
    pstack[j] = malloc(sizeof(mpz_t) * depth);
    qstack[j] = malloc(sizeof(mpz_t) * depth);
    gstack[j] = malloc(sizeof(mpz_t) * depth);
    for (i = 0; i < depth; i++) {
      mpz_init(pstack[j][i]);
      mpz_init(qstack[j][i]);
      mpz_init(gstack[j][i]);
    }
  }

  /* begin binary splitting process */
  if (terms <= 0) {
    mpz_set_ui(pstack[0][0], 1);
    mpz_set_ui(qstack[0][0], 0);
    mpz_set_ui(gstack[0][0], 1);
    for (i = 1; i < cores; i++) {
      mpz_clear(pstack[i][0]);
      mpz_clear(qstack[i][0]);
      mpz_clear(gstack[i][0]);
      free(pstack[i]);
      free(qstack[i]);
      free(gstack[i]);
    }
  } else {
#ifndef NO_FACTOR
    gcd = malloc(sizeof(mpz_t) * cores);
    ftmp = malloc(sizeof(fac_t) * cores);
    fmul = malloc(sizeof(fac_t) * cores);
    fpstack = malloc(sizeof(fac_t) * cores);
    fgstack = malloc(sizeof(fac_t) * cores);
    for (j = 0; j < cores; j++) {
      fpstack[j] = malloc(sizeof(fac_t) * depth);
      fgstack[j] = malloc(sizeof(fac_t) * depth);
      mpz_init(gcd[j]);
      fac_init(ftmp[j]);
      fac_init(fmul[j]);
      for (i = 0; i < depth; i++) {
        fac_init(fpstack[j][i]);
        fac_init(fgstack[j][i]);
      }
    }
#endif /* NO_FACTOR */

    mid0 = clock();
    wmid0 = wall_clock();

    mid = terms / cores;

#ifdef _OPENMP
#ifndef NO_FACTOR
#pragma omp parallel for default(shared) private(i) reduction(+ : gcd_time)
#else
#pragma omp parallel for default(shared) private(i)
#endif
#endif
    for (i = 0; i < cores; i++) {
      if (i < (cores - 1))
        bs(i * mid, (i + 1) * mid, 1, cores_depth, i, 0);
      else
        bs(i * mid, terms, 1, cores_depth, i, 0);
    }
    for (j = 0; j < cores; j++) {
      for (i = 1; i < depth; i++) {
        mpz_clear(pstack[j][i]);
        mpz_clear(qstack[j][i]);
        mpz_clear(gstack[j][i]);
      }
    }
#ifndef NO_FACTOR
    for (j = 0; j < cores; j++) {
      mpz_clear(gcd[j]);
      fac_clear(ftmp[j]);
      fac_clear(fmul[j]);
      for (i = 0; i < depth; i++) {
        fac_clear(fpstack[j][i]);
        fac_clear(fgstack[j][i]);
      }
      free(fpstack[j]);
      free(fgstack[j]);
    }
    free(gcd);
    free(ftmp);
    free(fmul);
    free(fpstack);
    free(fgstack);
#endif /* NO_FACTOR */

    for (k = 1; k < cores_size; k *= 2) {
#ifdef _OPENMP
#pragma omp parallel for default(shared) private(i)
#endif
      for (i = 0; i < cores; i = i + 2 * k) {
        if (i + k < cores) {
          sum(i, i + k, 1);
          mpz_clear(pstack[i + k][0]);
          mpz_clear(qstack[i + k][0]);
          mpz_clear(gstack[i + k][0]);
          free(pstack[i + k]);
          free(qstack[i + k]);
          free(gstack[i + k]);
        }
      }
    }
  }
  print_mpz(gstack[0][0], "g");

  mpz_clear(gstack[0][0]);
  free(gstack[0]);
  free(gstack);

  mid1 = clock();
  wmid1 = wall_clock();
  fprintf(stderr, "\nbs      cputime = %6.3f  wallclock = %6.3f\n",
          (double)(mid1 - mid0) / CLOCKS_PER_SEC, (wmid1 - wmid0));

#ifndef NO_FACTOR
  fprintf(stderr, "gcd     cputime = %6.3f\n",
          (double)(gcd_time) / CLOCKS_PER_SEC);

  // fprintf(stderr,"misc    "); fflush(stderr);

  /* free some resources */
  free(sieve);
#endif /* NO_FACTOR */

  /* prepare to convert integers to floats */
  mpf_set_default_prec((long int)(d * BITS_PER_DIGIT + 16));

  /*
                  p*(C/D)*sqrt(C)
    pi = -----------------
                  (q+A*p)
  */

  psize = mpz_sizeinbase(pstack[0][0], 10);
  qsize = mpz_sizeinbase(qstack[0][0], 10);

  print_mpz(qstack[0][0], "q");
  print_mpz(pstack[0][0], "p");

  mpz_addmul_ui(qstack[0][0], pstack[0][0], A);
  mpz_mul_ui(pstack[0][0], pstack[0][0], C / D);

  mpf_init(pi);
  mpf_set_z(pi, pstack[0][0]);
  mpz_clear(pstack[0][0]);

  mpf_init(qi);
  mpf_set_z(qi, qstack[0][0]);
  mpz_clear(qstack[0][0]);

  free(pstack[0]);
  free(qstack[0]);
  free(pstack);
  free(qstack);

  mid2 = clock();
  // fprintf(stderr,"cputime = %6.3f\n", (double)(mid2-mid1)/CLOCKS_PER_SEC);

  /* initialize temp float variables for sqrt & div */
  mpf_init(t1);
  mpf_init(t2);
  // mpf_set_prec_raw(t1, mpf_get_prec(pi));

  /* final step */
  fprintf(stderr, "div     ");
  fflush(stderr);
  my_div(qi, pi, qi);
  mid3 = clock();
  fprintf(stderr, "cputime = %6.3f\n", (double)(mid3 - mid2) / CLOCKS_PER_SEC);

  fprintf(stderr, "sqrt    ");
  fflush(stderr);
  my_sqrt_ui(pi, C);
  mid4 = clock();
  fprintf(stderr, "cputime = %6.3f\n", (double)(mid4 - mid3) / CLOCKS_PER_SEC);

  fprintf(stderr, "mul     ");
  fflush(stderr);
  mpf_mul(qi, qi, pi);
  end = clock();
  wend = wall_clock();
  fprintf(stderr, "cputime = %6.3f\n", (double)(end - mid4) / CLOCKS_PER_SEC);

  fprintf(stderr, "total   cputime = %6.3f  wallclock = %6.3f\n",
          (double)(end - begin) / CLOCKS_PER_SEC, (wend - wbegin));
  fflush(stderr);

  fprintf(stderr,
          "   P size=%ld digits (%f)\n"
          "   Q size=%ld digits (%f)\n",
          psize, (double)psize / d, qsize, (double)qsize / d);

  /* output Pi and timing statistics */
  if (out & 1) {
    fprintf(stdout, "pi(0,%ld)=\n", terms);
    mpf_out_str(stdout, 10, d + 2, qi);
    fprintf(stdout, "\n");
  }

  /* free float resources */
  mpf_clear(pi);
  mpf_clear(qi);

  mpf_clear(t1);
  mpf_clear(t2);
  exit(0);
}
