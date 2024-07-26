use std::ops::Mul;

use rayon::iter::{IntoParallelIterator, ParallelIterator};
use rug::{
    ops::{CompleteRound, NegAssign, Pow, PowAssign},
    Assign, Complete, Float, Integer,
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

fn chudnovsky(digits: u64) {
    /* number of serie terms */
    let n = (digits as f64 / 47.11041313821584202247).ceil() as u64 + 10;
    let prec1 = digits + 32;
}
