#[cfg(target_pointer_width = "64")]
const LIMB_DIGITS: usize = 19;
#[cfg(target_pointer_width = "64")]
const LIMB_LOG2_BITS: usize = 6;
#[cfg(target_pointer_width = "32")]
const LIMB_DIGITS: usize = 9;
#[cfg(target_pointer_width = "32")]
const LIMB_LOG2_BITS: usize = 5;

const LIMB_BITS: usize = 1 << LIMB_LOG2_BITS;

/// Minimum number of bits for the exponent
const BF_EXP_BITS_MIN: usize = 3;
/// Maximum number of bits for the exponent
const BF_EXP_BITS_MAX: usize = LIMB_BITS - 3;
/// Extended range for exponent, used internally
const BF_EXT_EXP_BITS_MAX: usize = BF_EXP_BITS_MAX + 1;
/// Minimum possible precision
const BF_PREC_MIN: usize = 2;
/// Maximum possible precision
const BF_PREC_MAX: usize = (1 << (LIMB_BITS - 2)) - 2;
/// Some operations support infinite precision
const BF_PREC_INF: usize = BF_PREC_MAX + 1;

const BF_RAW_EXP_MIN: isize = isize::MIN;
const BF_RAW_EXP_MAX: isize = isize::MAX;

const BF_EXP_ZERO: isize = BF_RAW_EXP_MIN;
const BF_EXP_INF: isize = BF_RAW_EXP_MAX - 1;
const BF_EXP_NAN: isize = BF_RAW_EXP_MAX;

pub enum Rounding {
  RoundToNearest,
  RoundToZero,
  RoundDown,
  RoundUp,
  RoundToNearestAwayFromZero,
  RoundAwayFromZero,
  RoundNondeterministic
}

// +- Zero is represented with expn = BF_EXP_ZERO and tab.len() = 0
// +- Infinity is represented with expn = BF_EXP_INF and tab.len() = 0
// NaN is represented with expn = BF_EXP_NAN and tab.len() = 0
pub struct BigFloat {
    sign: isize,
    expn: isize,
    tab: Vec<usize>,
}

impl BigFloat {
    pub fn new() -> Self {
        Self {
            sign: 0,
            expn: BF_EXP_ZERO,
            tab: vec![],
        }
    }

    pub fn set_ui(&mut self, a: usize) {
        self.sign = 0;
        if a == 0 {
            self.expn = isize::MIN;
            self.resize(0);
            
        } else {
            self.resize(1);
            let shift = a.leading_zeros();
            self.tab[0] = a << shift;
            self.expn = 64 - shift as i64;
        }
    }

    fn resize(&mut self, len: usize) {
        if len != self.tab.len() {
            self.tab.resize(len, 0);
        }
    }
}

impl std::fmt::Display for BigFloat {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.exponent == i64::MAX {
            write!(f, "NaN")?;
        } else {
            if self.sign < 0 {
                write!(f, "-")?;
            }
            if self.exponent == i64::MIN {
                write!(f, "0")?;
            } else if self.exponent == i64::MAX - 1 {
                write!(f, "Inf")?;
            } else {
                write!(f, "0x0.")?;
                for limb in self.tab.iter().rev() {
                    write!(f, "{:016}", limb)?;
                }
                write!(f, "p{}", self.exponent)?;
            }
        }
        writeln!(f)
    }
}
