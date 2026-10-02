use crate::error::{fail, ErrorCode, Result};

pub const IMPLEMENTATION_MAX_DEPTH: usize = 128;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Limits {
    pub max_bytes: usize,
    pub max_depth: usize,
    pub max_items: usize,
    pub max_string_length: usize,
}

impl Default for Limits {
    fn default() -> Self {
        Self {
            max_bytes: 65536,
            max_depth: 12,
            max_items: 2048,
            max_string_length: 8192,
        }
    }
}

impl Limits {
    pub fn check(&self) -> Result<()> {
        if self.max_depth > IMPLEMENTATION_MAX_DEPTH {
            return fail(ErrorCode::Limit);
        }
        Ok(())
    }
}
