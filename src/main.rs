use std::ops::Mul;

use rug::{
    ops::{CompleteRound, Pow},
    Complete, Float, Integer,
};

use rayon::{
    self,
    iter::{IntoParallelIterator, ParallelIterator},
};
fn main() {
    let digits: u32 = std::env::args()
        .nth(1)
        .map(|arg| {
            arg.parse()
                .expect("first argument should be a valid number")
        })
        .unwrap_or(60);

    chudnovsky(digits);
}

const DIGITS_PER_ITERATION: f32 = 14.1816474627254776555;
const BITS_PER_DIGIT: f32 = 3.32192809488736234789;

fn chudnovsky(digits: u32) {
    let iterations = ((digits as f32 / DIGITS_PER_ITERATION) + 1.) as u32;
    let precision_bits = ((digits as f32 * BITS_PER_DIGIT) + 1.) as u32;

    let mut sum: Float = (0..iterations)
        .into_par_iter()
        .map(|k| {
            let three_k = 3 * k;

            let a = Integer::factorial(6 * k).complete();
            let b = Integer::mul(545140134.into(), k) + 13591409;
            let c = Integer::factorial(three_k).complete();

            let d = Integer::factorial(k).complete().pow(3);

            let mut e = Integer::u_pow_u(640320, three_k).complete();
            if three_k & 1 == 1 {
                e = -e;
            }

            let numerator = Float::with_val(precision_bits, a * b);
            let denominator = Float::with_val(precision_bits, c * d * e);

            numerator / denominator
        })
        .reduce(|| Float::new(precision_bits), |stage, a| a + stage);

    let constant_part = Float::sqrt_u(10005).complete(precision_bits) * 426880;

    sum = 1 / sum * constant_part;

    let pi = sum.to_string();

    println!("{pi}");
}
