#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorCode {
    Malformed,
    DuplicateKey,
    Unicode,
    Number,
    Limit,
    Type,
    Version,
    Schema,
    Uint64Range,
    Float32,
    Timestamp,
    MetricUnit,
    MetricValue,
    OptionKeys,
    Profile,
    Algorithm,
    Integrity,
    Internal,
}

impl ErrorCode {
    pub fn as_str(&self) -> &'static str {
        match self {
            ErrorCode::Malformed => "MALFORMED",
            ErrorCode::DuplicateKey => "DUPLICATE_KEY",
            ErrorCode::Unicode => "UNICODE",
            ErrorCode::Number => "NUMBER",
            ErrorCode::Limit => "LIMIT",
            ErrorCode::Type => "TYPE",
            ErrorCode::Version => "VERSION",
            ErrorCode::Schema => "SCHEMA",
            ErrorCode::Uint64Range => "UINT64_RANGE",
            ErrorCode::Float32 => "FLOAT32",
            ErrorCode::Timestamp => "TIMESTAMP",
            ErrorCode::MetricUnit => "METRIC_UNIT",
            ErrorCode::MetricValue => "METRIC_VALUE",
            ErrorCode::OptionKeys => "OPTION_KEYS",
            ErrorCode::Profile => "PROFILE",
            ErrorCode::Algorithm => "ALGORITHM",
            ErrorCode::Integrity => "INTEGRITY",
            ErrorCode::Internal => "INTERNAL",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Error {
    pub code: ErrorCode,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.code.as_str())
    }
}

impl std::error::Error for Error {}

pub type Result<T> = std::result::Result<T, Error>;

#[inline]
pub fn fail<T>(code: ErrorCode) -> Result<T> {
    Err(Error { code })
}
