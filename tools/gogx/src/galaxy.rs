//! GOG Galaxy file parts.
//!
//! GOG's newer offline installers store each game file as one or more
//! zlib-compressed parts under hashed names in `{tmp}`, and say how to put
//! them back together in each part's BeforeInstall and AfterInstall
//! parameters, Pascal calls such as
//!
//! ```text
//! before_install('<md5 of the whole file>', '<real name>', <part count>)
//! after_install('<md5 of the part>', <stored size>, <real size>)
//! ```
//!
//! The first part of a file carries both; every later part carries only
//! `after_install`. Dependencies (`__redist/...`) use `before_install_dependency`
//! and `after_install_dependency` with the same arguments. innoextract
//! (`src/cli/goggalaxy.cpp`) is the reference for the format; like it, every
//! part is inflated and the whole file's MD5 is what is checked, the part MD5
//! being redundant with Inno's own checksum of the stored bytes.

use md5::{Digest, Md5};

/// The arguments of `code` when it is a call of `name`: `Ok(None)` when it
/// is empty or another call, an error when it is a call of `name` that does
/// not parse. Arguments are bare tokens or single-quoted strings, in which
/// `''` stands for one quote; a trailing `;` is allowed.
pub fn parse_call(code: &str, name: &str) -> Result<Option<Vec<String>>, String> {
    let text = code.trim();
    let ident_end = text
        .find(|c: char| !(c.is_ascii_alphanumeric() || c == '_'))
        .unwrap_or(text.len());
    if text.is_empty() || &text[..ident_end] != name {
        return Ok(None);
    }
    let bad = |why: &str| Err(format!("cannot parse {code:?}: {why}"));
    let mut rest = text[ident_end..].trim_start();
    if !rest.starts_with('(') {
        return Ok(None);
    }
    rest = rest[1..].trim_start();
    let mut args = Vec::new();
    if let Some(after) = rest.strip_prefix(')') {
        rest = after;
    } else {
        loop {
            let (arg, after) = if let Some(quoted) = rest.strip_prefix('\'') {
                let mut value = String::new();
                let mut chars = quoted.char_indices();
                let mut end = None;
                while let Some((i, c)) = chars.next() {
                    if c != '\'' {
                        value.push(c);
                    } else if quoted[i + 1..].starts_with('\'') {
                        value.push('\'');
                        chars.next();
                    } else {
                        end = Some(i + 1);
                        break;
                    }
                }
                match end {
                    Some(end) => (value, &quoted[end..]),
                    None => return bad("an unterminated string"),
                }
            } else {
                let end = rest
                    .find(|c: char| c.is_whitespace() || "(),'".contains(c))
                    .unwrap_or(rest.len());
                if end == 0 {
                    return bad("an empty argument");
                }
                (rest[..end].to_string(), &rest[end..])
            };
            args.push(arg);
            rest = after.trim_start();
            if let Some(after) = rest.strip_prefix(',') {
                rest = after.trim_start();
            } else if let Some(after) = rest.strip_prefix(')') {
                rest = after;
                break;
            } else {
                return bad("expected ',' or ')'");
            }
        }
    }
    let rest = rest.trim();
    if !(rest.is_empty() || rest == ";") {
        return bad("text after the call");
    }
    Ok(Some(args))
}

/// The start of a multi-part file.
#[derive(Debug, PartialEq, Eq)]
pub struct Start {
    pub md5: [u8; 16],
    pub name: String,
    pub parts: u32,
}

/// One part's sizes: as stored in the installer, and inflated.
#[derive(Debug, PartialEq, Eq)]
pub struct Part {
    pub stored: u64,
    pub size: u64,
}

/// What an installer entry is, from its two parameters.
#[derive(Debug, PartialEq, Eq)]
pub enum Kind {
    /// An ordinary file at its own destination.
    Plain,
    /// A Galaxy part, the first of its file when `start` is given.
    Part { start: Option<Start>, part: Part },
}

fn either(code: &str, name: &str) -> Result<Option<Vec<String>>, String> {
    match parse_call(code, name)? {
        Some(args) => Ok(Some(args)),
        None => parse_call(code, &format!("{name}_dependency")),
    }
}

fn md5_hex(text: &str) -> Result<[u8; 16], String> {
    let bytes = text.as_bytes();
    if bytes.len() != 32 {
        return Err(format!("{text:?} is not an MD5"));
    }
    let mut out = [0u8; 16];
    for (i, pair) in bytes.chunks(2).enumerate() {
        let pair = std::str::from_utf8(pair).map_err(|_| format!("{text:?} is not an MD5"))?;
        out[i] = u8::from_str_radix(pair, 16).map_err(|_| format!("{text:?} is not an MD5"))?;
    }
    Ok(out)
}

fn number(text: &str, what: &str) -> Result<u64, String> {
    text.parse().map_err(|_| format!("{what} {text:?} is not a number"))
}

/// Classifies an entry by its BeforeInstall and AfterInstall parameters.
pub fn classify(before: &str, after: &str) -> Result<Kind, String> {
    let start = match either(before, "before_install")? {
        None => None,
        Some(args) => {
            let [md5, name, parts] = args.as_slice() else {
                return Err(format!("before_install with {} arguments, not 3: {before:?}", args.len()));
            };
            if name.is_empty() {
                return Err(format!("before_install with no file name: {before:?}"));
            }
            let parts = number(parts, "part count")?;
            Some(Start {
                md5: md5_hex(md5)?,
                name: name.clone(),
                // innoextract reads a count of 0 as 1.
                parts: u32::try_from(parts.max(1)).map_err(|_| format!("part count {parts} is too large"))?,
            })
        }
    };
    match either(after, "after_install")? {
        None if start.is_some() => Err(format!("before_install without after_install: {before:?}")),
        None => Ok(Kind::Plain),
        Some(args) => {
            let [_md5, stored, size] = args.as_slice() else {
                return Err(format!("after_install with {} arguments, not 3: {after:?}", args.len()));
            };
            Ok(Kind::Part {
                start,
                part: Part { stored: number(stored, "stored size")?, size: number(size, "size")? },
            })
        }
    }
}

/// Follows the parts of one file at a time, in installer order.
#[derive(Default)]
pub struct Reassembly {
    open: Option<Open>,
}

struct Open {
    name: String,
    md5: [u8; 16],
    remaining: u32,
    hasher: Md5,
    size: u64,
}

impl Reassembly {
    /// The name of the file being put together, if one is.
    pub fn current(&self) -> Option<&str> {
        self.open.as_ref().map(|o| o.name.as_str())
    }

    /// Opens a file; an error if the previous one is unfinished.
    pub fn start(&mut self, start: Start) -> Result<(), String> {
        if let Some(open) = &self.open {
            return Err(format!("{} ends after {} of its parts are missing", open.name, open.remaining));
        }
        self.open = Some(Open {
            name: start.name,
            md5: start.md5,
            remaining: start.parts,
            hasher: Md5::new(),
            size: 0,
        });
        Ok(())
    }

    /// Takes the next part's inflated bytes, in one or more calls.
    pub fn feed(&mut self, bytes: &[u8]) -> Result<(), String> {
        let open = self.open.as_mut().ok_or("a GOG Galaxy part with no file started")?;
        open.hasher.update(bytes);
        open.size += bytes.len() as u64;
        Ok(())
    }

    /// Ends a part. Returns the file's name and size when it was the last
    /// part, after checking the whole file's MD5.
    pub fn end_part(&mut self) -> Result<Option<(String, u64)>, String> {
        let open = self.open.as_mut().ok_or("a GOG Galaxy part with no file started")?;
        open.remaining -= 1;
        if open.remaining > 0 {
            return Ok(None);
        }
        let open = self.open.take().expect("checked above");
        let actual: [u8; 16] = open.hasher.finalize().into();
        if actual != open.md5 {
            return Err(format!(
                "{} does not match its MD5: {} expected, {} read",
                open.name,
                hex(&open.md5),
                hex(&actual)
            ));
        }
        Ok(Some((open.name, open.size)))
    }

    /// An error if a file is still waiting for parts.
    pub fn finish(&self) -> Result<(), String> {
        match &self.open {
            Some(open) => Err(format!("{} is missing its last {} parts", open.name, open.remaining)),
            None => Ok(()),
        }
    }
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn args(v: &[&str]) -> Option<Vec<String>> {
        Some(v.iter().map(|s| s.to_string()).collect())
    }

    #[test]
    fn calls_parse_as_gog_writes_them() {
        assert_eq!(
            parse_call("before_install('23a4883b94f4217bf7e2412ddde86632', 'arcanum1.dat', 53)", "before_install"),
            Ok(args(&["23a4883b94f4217bf7e2412ddde86632", "arcanum1.dat", "53"]))
        );
        assert_eq!(
            parse_call("  after_install ( 'aa' ,8391160,8388608 ) ; ", "after_install"),
            Ok(args(&["aa", "8391160", "8388608"]))
        );
        assert_eq!(parse_call("f()", "f"), Ok(Some(vec![])));
    }

    #[test]
    fn quotes_commas_and_parentheses_inside_strings_survive() {
        assert_eq!(
            parse_call("before_install('x', 'Tycho''s Tomb, (Act 2).mp3', 1)", "before_install"),
            Ok(args(&["x", "Tycho's Tomb, (Act 2).mp3", "1"]))
        );
        assert_eq!(parse_call("f('''')", "f"), Ok(args(&["'"])));
    }

    #[test]
    fn other_calls_and_empty_text_are_not_this_call() {
        assert_eq!(parse_call("", "before_install"), Ok(None));
        assert_eq!(parse_call("before_install_dependency('a', 'b', 1)", "before_install"), Ok(None));
        assert_eq!(parse_call("check_if_install('en-US#')", "before_install"), Ok(None));
        assert_eq!(parse_call("before_installer('a')", "before_install"), Ok(None));
    }

    #[test]
    fn malformed_calls_are_errors() {
        assert!(parse_call("f('a", "f").is_err());
        assert!(parse_call("f('a' 'b')", "f").is_err());
        assert!(parse_call("f(a,)", "f").is_err());
        assert!(parse_call("f(a); g()", "f").is_err());
        assert!(parse_call("f(a", "f").is_err());
    }

    const MD5: &str = "0123456789abcdef0123456789abcdef";

    #[test]
    fn entries_classify_by_their_parameters() {
        assert_eq!(classify("", ""), Ok(Kind::Plain));
        let first = classify(&format!("before_install('{MD5}', 'a/b.dat', 2)"), "after_install('x', 10, 20)");
        let Ok(Kind::Part { start: Some(start), part }) = first else { panic!("{first:?}") };
        assert_eq!((start.name.as_str(), start.parts), ("a/b.dat", 2));
        assert_eq!(part, Part { stored: 10, size: 20 });
        assert_eq!(
            classify("", "after_install_dependency('x', 1, 2)"),
            Ok(Kind::Part { start: None, part: Part { stored: 1, size: 2 } })
        );
        let zero = classify(&format!("before_install('{MD5}', 'n', 0)"), "after_install('x', 1, 1)");
        assert!(matches!(zero, Ok(Kind::Part { start: Some(Start { parts: 1, .. }), .. })));
    }

    #[test]
    fn incomplete_or_wrong_parameters_are_errors() {
        assert!(classify(&format!("before_install('{MD5}', 'n', 1)"), "").is_err());
        assert!(classify("before_install('nothex', 'n', 1)", "after_install('x', 1, 1)").is_err());
        assert!(classify(&format!("before_install('{MD5}', '', 1)"), "after_install('x', 1, 1)").is_err());
        assert!(classify(&format!("before_install('{MD5}', 'n')"), "after_install('x', 1, 1)").is_err());
        assert!(classify("", "after_install('x', 'big', 1)").is_err());
    }

    fn start(name: &str, data: &[u8], parts: u32) -> Start {
        Start { md5: Md5::digest(data).into(), name: name.into(), parts }
    }

    #[test]
    fn parts_reassemble_and_check_the_whole_files_md5() {
        let mut r = Reassembly::default();
        r.start(start("f", b"hello world", 2)).unwrap();
        r.feed(b"hello ").unwrap();
        assert_eq!(r.end_part(), Ok(None));
        r.feed(b"world").unwrap();
        assert_eq!(r.end_part(), Ok(Some(("f".to_string(), 11))));
        assert!(r.finish().is_ok());
    }

    #[test]
    fn a_wrong_md5_or_a_missing_part_is_an_error() {
        let mut r = Reassembly::default();
        r.start(start("f", b"hello", 1)).unwrap();
        r.feed(b"hellO").unwrap();
        assert!(r.end_part().is_err());

        let mut r = Reassembly::default();
        r.start(start("f", b"ab", 2)).unwrap();
        r.feed(b"a").unwrap();
        r.end_part().unwrap();
        assert!(r.finish().is_err());
        assert!(r.start(start("g", b"", 1)).is_err());

        let mut r = Reassembly::default();
        assert!(r.feed(b"x").is_err());
        assert!(r.end_part().is_err());
    }
}
