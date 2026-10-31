//! Where an entry is written, inside the output folder and nowhere else.
//!
//! Names follow innoextract's layout, which `setup.sh` expects: a plain
//! entry's leading Inno constant loses its braces (`{app}\x` becomes
//! `app/x`), a GOG Galaxy file goes to its real name at the top, and plain
//! entries under `{tmp}` are left out, as `innoextract --exclude-temp` does.

use std::collections::HashSet;
use std::path::PathBuf;

/// A safe relative path from `/`- or `\`-separated text: no empty name, no
/// leading separator, no `.` or `..`, no drive or stream colon, no Inno
/// constant left unexpanded, no control character.
pub fn relative(text: &str) -> Result<PathBuf, String> {
    let bad = |why: &str| Err(format!("refusing to write {text:?}: {why}"));
    if text.starts_with(['/', '\\']) {
        return bad("an absolute path");
    }
    let mut path = PathBuf::new();
    for part in text.split(['/', '\\']) {
        match part {
            "" => continue,
            "." | ".." => return bad("a '.' or '..' component"),
            _ if part.contains(':') => return bad("a drive or stream name"),
            _ if part.contains(['{', '}']) => return bad("an unexpanded Inno Setup constant"),
            _ if part.chars().any(char::is_control) => return bad("a control character"),
            _ => path.push(part),
        }
    }
    if path.as_os_str().is_empty() {
        return bad("an empty name");
    }
    Ok(path)
}

/// The output path of a plain entry, or `None` for a `{tmp}` entry.
pub fn plain(destination: &str) -> Result<Option<PathBuf>, String> {
    let text = match destination.strip_prefix('{') {
        Some(rest) => {
            let Some(end) = rest.find('}') else {
                return relative(destination).map(Some);
            };
            let (constant, tail) = (&rest[..end], &rest[end + 1..]);
            if constant.eq_ignore_ascii_case("tmp") {
                return Ok(None);
            }
            format!("{constant}/{tail}")
        }
        None => destination.to_string(),
    };
    relative(&text).map(Some)
}

/// Refuses a second write to one path, compared without case, since the
/// Mac's file system ignores case by default.
#[derive(Default)]
pub struct Claimed(HashSet<String>);

impl Claimed {
    pub fn claim(&mut self, path: &std::path::Path) -> Result<(), String> {
        let key = path.to_string_lossy().to_lowercase();
        if self.0.insert(key) {
            Ok(())
        } else {
            Err(format!(
                "two files would be written to {} (names compared without case); \
                 a multi-language installer, which gogx does not choose between",
                path.display()
            ))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn plain_entries_lose_the_constants_braces() {
        assert_eq!(plain("{app}\\data\\x.dat"), Ok(Some(PathBuf::from("app/data/x.dat"))));
        assert_eq!(
            plain("{commonappdata}\\GOG.com\\u.dll"),
            Ok(Some(PathBuf::from("commonappdata/GOG.com/u.dll")))
        );
        assert_eq!(plain("{tmp}\\27\\1c\\271c"), Ok(None));
        assert_eq!(plain("{TMP}\\a"), Ok(None));
        assert_eq!(plain("plain\\name"), Ok(Some(PathBuf::from("plain/name"))));
    }

    #[test]
    fn galaxy_names_are_relative_paths() {
        assert_eq!(relative("__redist/ISI/scriptinterpreter.exe"), Ok(PathBuf::from("__redist/ISI/scriptinterpreter.exe")));
        assert_eq!(relative("modules\\Arcanum\\x.dat"), Ok(PathBuf::from("modules/Arcanum/x.dat")));
        assert_eq!(relative("data//sound/Combat 1.mp3"), Ok(PathBuf::from("data/sound/Combat 1.mp3")));
    }

    #[test]
    fn paths_that_leave_the_folder_are_refused() {
        for text in ["../x", "a/../../x", "/etc/x", "\\x", "C:\\x", "a/./b", "", "/", "a\\..", "x:y", "a/\u{1}b"] {
            assert!(relative(text).is_err(), "{text:?} accepted");
        }
        assert!(plain("{app}\\..\\..\\x").is_err());
        assert!(plain("{app}\\{code:Dir}\\x").is_err());
        assert!(plain("{app").is_err());
    }

    #[test]
    fn a_second_write_to_one_path_is_refused_whatever_its_case() {
        let mut claimed = Claimed::default();
        assert!(claimed.claim(&PathBuf::from("modules/Arcanum.dat")).is_ok());
        assert!(claimed.claim(&PathBuf::from("Modules/arcanum.DAT")).is_err());
        assert!(claimed.claim(&PathBuf::from("modules/other.dat")).is_ok());
    }
}
