use std::ops::Mul;

use rayon::iter::{IntoParallelIterator, ParallelIterator};
use rug::{
    ops::{CompleteRound, NegAssign, Pow, PowAssign}, Assign, Complete, Float, Integer
};

const A: u32 = 13591409;
const B: u32 = 545140134;
const C: u32 = 640320;
const D: u32 = 12;

const DIGITS_PER_ITER: f64 = 14.1816474627254776555;
const BITS_PER_DIGIT: f64 = 3.32192809488736234787;

type PQG = (Integer, Integer, Integer);

fn main() {
    let digits: usize = std::env::args()
        .nth(1)
        .map(|arg| {
            arg.replace("_", "")
                .parse()
                .expect("digits should be a valid number")
        })
        .unwrap_or(60); // calculate 60 digits by default

    let threads: usize = std::env::args()
        .nth(2)
        .map(|arg| arg.parse().expect("threads should be a valid number"))
        .unwrap_or(1); // use 1 core by default

    let pi = chudnovsky(digits, threads);

    println!("{pi}");
}

fn sum(a: PQG, b: PQG) -> PQG {
    let (mut p, mut q, mut g) = a;
    let (in_p, in_q, in_g) = b;

    p *= &in_p;
    q *= in_p;
    q += in_q * &g;

    g *= in_g;

    (p, q, g)
}

fn reduce(arr: &mut [PQG]) -> PQG {
    match arr.len() {
        1 => std::mem::take(&mut arr[0]),
        2 => {
            let a = std::mem::take(&mut arr[0]);
            let b = std::mem::take(&mut arr[1]);
            sum(a, b)
        }
        n => {
            let mid = n / 2;
            let (start, end) = arr.split_at_mut(mid);
            let (a, b) = rayon::join(|| reduce(start), || reduce(end));

            sum(a, b)
        }
    }
}

fn chudnovsky(num_digits: usize, mut threads: usize) -> String {
    let iters_needed = (num_digits as f64 / DIGITS_PER_ITER) as usize;

    if iters_needed < threads {
        threads = iters_needed;
    }

    let pool = rayon::ThreadPoolBuilder::new()
        .num_threads(threads)
        .use_current_thread()
        .build()
        .unwrap();

    let mut depth = 0;

    while 1 << depth < iters_needed {
        depth += 1;
    }
    depth += 1;

    let iters_per_thread = iters_needed / threads;

    let mut arr: Vec<PQG> = pool
        .install(|| {
            (0..threads).into_par_iter().map(|i| {
                let splitter = CoreSplitter::new(depth);
                let (p, q, g) = if i < threads - 1 {
                    splitter.binary_split(i * iters_per_thread, (i + 1) * iters_per_thread)
                } else {
                    splitter.binary_split(i * iters_per_thread, iters_needed)
                };
                (p, q, g)
            })
        })
        .collect();

    eprintln!("Done splitting");

    let (mut p, mut q, _) = pool.install(|| reduce(&mut arr));

    /*
    p*(C/D)*sqrt(C)
    pi = -----------------
    (q+A*p)
    */

    eprintln!("Done summing");

    q += &p * A;
    p *= C / D;
    eprintln!("Done q,p");

    let target_prec = (num_digits as f64 * BITS_PER_DIGIT + 16.) as u32;
    let mut p_float = Float::with_val(target_prec, p);

    let mut q_float = Float::with_val(target_prec, q);

    q_float = &p_float / q_float;
    eprintln!("Done pf/qf");

    Float::sqrt_u(C).complete_into(&mut p_float);

    q_float *= p_float;
    eprintln!("Done q*p");

    let mut s = q_float.to_string();
    s.truncate("3.".len() + num_digits); // remove possibly inaccurate numbers

    s
}



struct CoreSplitter {
    p_stack: Vec<Integer>,
    q_stack: Vec<Integer>,
    g_stack: Vec<Integer>,
}

impl CoreSplitter {
    fn new(depth: usize) -> Self {
        Self {
            p_stack: vec![Integer::new(); depth],
            g_stack: vec![Integer::new(); depth],
            q_stack: vec![Integer::new(); depth],
        }
    }

    fn binary_split(mut self, from: usize, to: usize) -> PQG {
        self._bs(from, to, 0, 0);
        let p = self.p_stack.swap_remove(0);
        let q = self.q_stack.swap_remove(0);
        let g = self.g_stack.swap_remove(0);

        (p, q, g)
    }

    fn _bs(&mut self, a: usize, b: usize, level: usize, top: usize) {
        //eprintln!(
        // "bs: a = {a}, b = {b}, gflag = {g_flag} index = {index} level = {level} top = {top}"
        // );

        if b > a && b - a == 1 {
            /*
              g(b-1,b) = (6b-5)(2b-1)(6b-1)
              p(b-1,b) = b^3 * C^3 / 24
              q(b-1,b) = (-1)^b*g(b-1,b)*(A+Bb).
            */
            let p1 = &mut self.p_stack[top];
            let q1 = &mut self.q_stack[top];
            let g1 = &mut self.g_stack[top];

            p1.assign(b);
            p1.pow_assign(3);
            *p1 *= (C / 24) * (C / 24);
            *p1 *= C * 24;

            g1.assign(2 * b - 1);
            *g1 *= 6 * b - 1;
            *g1 *= 6 * b - 5;

            q1.assign(b);
            *q1 *= B;
            *q1 += A;
            *q1 *= &*g1;

            if b % 2 == 1 {
                q1.neg_assign();
            }
        } else {
            /*
            p(a,b) = p(a,m) * p(m,b)
            q(a,b) = q(a,m) * p(m,b) + q(m,b) * g(a,m)
            g(a,b) = g(a,m) * g(m,b)
            */
            let m = (a as f32 + (b as f32 - a as f32) * 0.5224) as usize; // tuning parameter

            self._bs(a, m, level + 1, top);

            self._bs(m, b, level + 1, top + 1);

            #[rustfmt::skip]
            let [p_am, p_mb] = &mut self.p_stack[top..=top+1] else { unreachable!()};
            #[rustfmt::skip]
            let [g_am, g_mb] = &mut self.g_stack[top..=top+1] else { unreachable!()};
            #[rustfmt::skip]
            let [q_am, q_mb] = &mut self.q_stack[top..=top+1] else { unreachable!()};

            *p_am *= &*p_mb;

            *q_am *= &*p_mb;
            *q_am += &*q_mb * &*g_am;
            // *q_am = (&*q_am * &*p_mb).complete() + (&*q_mb * &*g_am).complete();
            *g_am *= &*g_mb;
        }
    }
}


fn chudnovsky_simple(digits: usize) -> String {
    // constant for Chudnosvky's algorithm:
    const DIGITS_PER_ITERATION: f32 = 14.1816474627254776555;
    // roughly compute how many bits of precision we need for
    // this many digit:
    const BITS_PER_DIGIT: f32 = 3.32192809488736234789; // log2(10)

	// add extra iterations to avoid rounding errors in the final output
    let iterations = ((digits as f32 / DIGITS_PER_ITERATION) + 2.) as u32;
    let precision_bits = ((digits as f32 * BITS_PER_DIGIT) + 2.) as u32;

    let mut sum = Float::new(precision_bits);
    for k in 0..iterations {
        //                               _____
        //                     426880 * /10005
        //  pi = ---------------------------------------------
        //         _inf_
        //         \     (6*k)! * (13591409 + 545140134 * k)
        //          \    -----------------------------------
        //          /     (3*k)! * (k!)^3 * (-640320)^(3*k)
        //         /____
        //          k=0
        //

        let three_k = 3 * k;

        let a = Integer::factorial(6 * k).complete(); // (6k)!
        let b = Integer::mul(545140134.into(), k) + 13591409; // 545140134*k + 13591409

        let c = Integer::factorial(three_k).complete(); // (3k)!
        let d = Integer::factorial(k).complete().pow(3); // (k!)^3
        let mut e = Integer::u_pow_u(640320, three_k).complete(); // -640320^(3k)

        if three_k % 2 == 1 {
            e = -e;
        }

        let numerator = Float::with_val(precision_bits, a * b);
        let denominator = Float::with_val(precision_bits, c * d * e);

        sum += numerator / denominator
    }

    // now we just need to invert the resulting sum and multiply it by the constant part

    let constant_part = Float::sqrt_u(10005).complete(precision_bits) * 426880;
    sum = 1 / sum * constant_part;

    let mut s = sum.to_string();
    s.truncate("3.".len() + digits); // remove possibly inaccurate numbers
    
	s
}

#[cfg(test)]
mod tests {
    use crate::chudnovsky;

    #[test]
    fn verify_last_10_digits() {
        // http://www.numberworld.org/digits/Pi/
        for (digits, expected_last_10) in [
            (100, "3421170679"),
            (10_000, "5256375678"),
            (500_000, "5138195242"),
            (1_000_000, "5779458151"),
        ] {
            let pi = chudnovsky(digits, 1);
            let actual_last_10 = pi
                .char_indices()
                .rev()
                .nth(9)
                .map(|(i, _)| &pi[i..])
                .expect("should have more than 10 characters");

            assert_eq!(
                actual_last_10, expected_last_10,
                "testing {digits} digits of pi (right = expected)"
            );
        }
    }
    #[test]
    fn verify_multithreaded() {
        let threads = std::thread::available_parallelism()
            .map(|n| n.get())
            .unwrap_or(4);

        // http://www.numberworld.org/digits/Pi/
        for (digits, expected_last_10) in [
            (100, "3421170679"),
            (10_000, "5256375678"),
            (500_000, "5138195242"),
            (1_000_000, "5779458151"),
        ] {
            let pi = chudnovsky(digits, threads);
            let actual_last_10 = pi
                .char_indices()
                .rev()
                .nth(9)
                .map(|(i, _)| &pi[i..])
                .expect("should have more than 10 characters");

            assert_eq!(
                actual_last_10, expected_last_10,
                "testing {digits} digits of pi with {threads} cores (right = expected)"
            );
        }
    }
}
