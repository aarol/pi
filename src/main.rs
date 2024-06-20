use std::ops::{AddAssign, Mul, MulAssign};

use rug::{
    ops::{CompleteRound, NegAssign, PowAssign},
    Assign, Complete, Float, Integer,
};

const A: u32 = 13591409;
const B: u32 = 545140134;
const C: u32 = 640320;
const D: u32 = 12;

const DIGITS_PER_ITER: f64 = 14.1816474627254776555;
const BITS_PER_DIGIT: f64 = 3.32192809488736234787;

fn main() {
    let digits: u32 = std::env::args()
        .nth(1)
        .map(|arg| {
            arg.parse()
                .expect("first argument should be a valid number")
        })
        .unwrap_or(60);

    let pi = chudnovsky(digits);

    println!("{pi}");
}

fn chudnovsky(digits: u32) -> String {
    let terms = (digits as f64 / DIGITS_PER_ITER) as usize;

    let mut depth = 0;
    while 1 << depth < terms {
        depth += 1;
    }
    depth += 1;

    let cores = 1;
    let mut cores_depth = 0;
    while 1 << cores_depth < cores {
        cores_depth += 1;
    }

    //eprintln!("#terms={terms}, depth={depth}, cores={cores}");

    let mut splitter = CoreSplitter::new(depth);

    splitter.bs(0, terms as u32, 1, cores_depth as usize, 0, 0);

    let default_prec = (digits as f64 * BITS_PER_DIGIT + 16.) as u32;

    /*
                p*(C/D)*sqrt(C)
      pi = -----------------
                (q+A*p)
    */
    let p1 = &mut splitter.p_stack[0];
    let q1 = &mut splitter.q_stack[0];

    q1.add_assign(p1.clone().mul(A));
    p1.mul_assign(C / D);

    let mut pi = Float::new(default_prec);
    pi.assign(&*p1);

    let mut qi = Float::new(default_prec);
    qi.assign(&*q1);

    qi = pi / qi;

    pi = Float::sqrt_u(C).complete(default_prec);

    qi *= pi;

    qi.to_string()[0..(digits as usize + 2)].to_owned()
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

    fn bs(&mut self, a: u32, b: u32, g_flag: usize, level: usize, index: usize, top: usize) {
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
            g(a,b) = g(a,m) * g(m,b)
            q(a,b) = q(a,m) * p(m,b) + q(m,b) * g(a,m)
            */
            let mid = (a as f32 + (b as f32 - a as f32) * 0.5224) as u32; // tuning parameter
            self.bs(a, mid, 1, level + 1, index, top);

            self.bs(mid, b, g_flag, level + 1, index, top + 1);

            self.p_stack[top] = (&self.p_stack[top] * &self.p_stack[top + 1]).complete();
            self.q_stack[top] = (&self.q_stack[top] * &self.p_stack[top + 1]).complete();
            self.q_stack[top + 1] = (&self.q_stack[top + 1] * &self.g_stack[top]).complete();
            //eprintln!("q_bs_top={}", self.q_stack[top]);
            self.q_stack[top] = (&self.q_stack[top] + &self.q_stack[top + 1]).into();
            //eprintln!("q_bs_bot={}", self.q_stack[top]);

            if g_flag == 1 {
                self.g_stack[top] = (&self.g_stack[top] * &self.g_stack[top + 1]).complete();
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::chudnovsky;

    #[test]
    fn run() {
        for (digits, expected_last_10) in [
            (100, "3421170679"),
            (10_000, "5256375678"),
            (500_000, "5138195242"),
            (1_000_000, "5779458151")
        ] {
            let pi = chudnovsky(digits);
            let actual_last_10 = pi
                .char_indices()
                .rev()
                .nth(9)
                .map(|(i, _)| &pi[i..])
                .expect("should have more than 10 characters");

            assert_eq!(
                actual_last_10, expected_last_10,
                "testing {digits} digits of Pi"
            );
        }
    }
}
