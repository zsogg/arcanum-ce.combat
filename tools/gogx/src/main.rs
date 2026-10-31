//! gogx: unpacks a GOG.com offline game installer (Inno Setup), so that
//! Arcanum CE's macOS setup needs no package manager.
//!
//!     gogx INSTALLER DIR    extract into DIR, which must be new or empty
//!     gogx --test INSTALLER decode and check everything, write nothing
//!     gogx --version
//!
//! The output is innoextract's layout with `--exclude-temp` (paths.rs), and
//! GOG Galaxy parts are reassembled into their files (galaxy.rs). Every
//! file is checked: Inno Setup's own checksum of each stored file, and for
//! a reassembled file the MD5 GOG records. Anything gogx does not
//! understand is an error, never a skipped file; the exit status is 0 only
//! when every file was written and checked.
//!
//! Built on the innospect crate with three fixes (patches/); see README.md.

mod galaxy;
mod paths;

use std::fs::{self, File};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use innospect::{Error as InnoError, InnoInstaller};

const BIN_ADVICE: &str = "this installer keeps its data in separate .bin files, which gogx cannot \
     read; install the game with GOG's installer on a Windows PC and give setup the game folder \
     instead (ARCANUM_SETUP_GOG_DIR)";

/// Where the files go: a folder, or nowhere (`--test`).
enum Output {
    Dir(PathBuf),
    Test,
}

impl Output {
    fn create(&self, rel: &Path) -> Result<Box<dyn Write>, String> {
        match self {
            Output::Test => Ok(Box::new(io::sink())),
            Output::Dir(dir) => {
                let path = dir.join(rel);
                if let Some(parent) = path.parent() {
                    fs::create_dir_all(parent).map_err(|e| format!("{}: {e}", parent.display()))?;
                }
                // create_new: never replace a file, whatever Claimed missed.
                let file = File::options()
                    .write(true)
                    .create_new(true)
                    .open(&path)
                    .map_err(|e| format!("{}: {e}", path.display()))?;
                Ok(Box::new(io::BufWriter::with_capacity(1 << 20, file)))
            }
        }
    }
}

fn describe(err: &InnoError) -> String {
    match err {
        InnoError::ExternalSlice | InnoError::MultiSliceChunk { .. } => BIN_ADVICE.to_string(),
        InnoError::Encrypted | InnoError::PasswordRequired => {
            "the installer is password-protected; GOG's are not, so this is not one".to_string()
        }
        other => other.to_string(),
    }
}

struct Totals {
    files: u64,
    bytes: u64,
}

fn extract(inst: &InnoInstaller<'_>, out: &Output) -> Result<Totals, String> {
    if inst.header().is_none() {
        return Err("the installer's setup data could not be read (an Inno Setup older than \
                    4.0.9, or encrypted)"
            .to_string());
    }
    if inst.offset_table().offset_setup1 == 0 {
        return Err(BIN_ADVICE.to_string());
    }
    if !inst.files().iter().any(|f| f.location_index != u32::MAX) {
        return Err("the installer lists no files".to_string());
    }

    let mut claimed = paths::Claimed::default();
    let mut galaxy = galaxy::Reassembly::default();
    let mut writer: Option<Box<dyn Write>> = None;
    let mut totals = Totals { files: 0, bytes: 0 };

    for file in inst.files() {
        if file.location_index == u32::MAX {
            continue; // the uninstaller stub: no data of its own
        }
        let conditions = &file.item.conditions;
        let kind = galaxy::classify(&conditions.before_install, &conditions.after_install)
            .map_err(|e| format!("{}: {e}", file.destination))?;
        match kind {
            galaxy::Kind::Plain => {
                if let Some(name) = galaxy.current() {
                    return Err(format!("{name} is interrupted by {} before its last part", file.destination));
                }
                let Some(rel) = paths::plain(&file.destination)? else {
                    continue; // {tmp}: the installer's own furniture
                };
                claimed.claim(&rel)?;
                let mut reader = inst.extract(file).map_err(|e| format!("{}: {}", file.destination, describe(&e)))?;
                let mut sink = out.create(&rel)?;
                // io::copy reads to the end, which is where the reader checks
                // Inno's checksum; a mismatch comes back as an error here.
                let n = io::copy(&mut reader, &mut sink).map_err(|e| format!("{}: {e}", rel.display()))?;
                sink.flush().map_err(|e| format!("{}: {e}", rel.display()))?;
                totals.files += 1;
                totals.bytes += n;
            }
            galaxy::Kind::Part { start, part } => {
                if let Some(start) = start {
                    let rel = paths::relative(&start.name)?;
                    claimed.claim(&rel)?;
                    galaxy.start(start)?;
                    writer = Some(out.create(&rel)?);
                }
                let name = galaxy.current().ok_or_else(|| {
                    format!("{}: a GOG Galaxy part with no file started", file.destination)
                })?;
                let name = name.to_string();
                // extract_to_vec has checked Inno's checksum of the stored part.
                let stored = inst.extract_to_vec(file).map_err(|e| format!("{name}: {}", describe(&e)))?;
                if stored.len() as u64 != part.stored {
                    return Err(format!(
                        "{name}: a part holds {} bytes where GOG records {}",
                        stored.len(),
                        part.stored
                    ));
                }
                let mut inflater = flate2::read::ZlibDecoder::new(stored.as_slice());
                let mut buf = vec![0u8; 1 << 16];
                let mut inflated = 0u64;
                let sink = writer.as_mut().expect("a writer exists while a file is open");
                loop {
                    let n = inflater.read(&mut buf).map_err(|e| format!("{name}: inflating a part: {e}"))?;
                    if n == 0 {
                        break;
                    }
                    // Checked as it grows: a corrupt part must not fill the disk first.
                    if inflated + n as u64 > part.size {
                        return Err(format!("{name}: a part inflates past the {} bytes GOG records", part.size));
                    }
                    galaxy.feed(&buf[..n])?;
                    sink.write_all(&buf[..n]).map_err(|e| format!("{name}: {e}"))?;
                    inflated += n as u64;
                }
                if inflater.total_in() != stored.len() as u64 {
                    return Err(format!("{name}: a part has data after its zlib stream"));
                }
                if inflated != part.size {
                    return Err(format!("{name}: a part inflates to {inflated} bytes where GOG records {}", part.size));
                }
                if let Some((_, size)) = galaxy.end_part()? {
                    let mut done = writer.take().expect("a writer exists while a file is open");
                    done.flush().map_err(|e| format!("{name}: {e}"))?;
                    totals.files += 1;
                    totals.bytes += size;
                }
            }
        }
    }
    galaxy.finish()?;
    Ok(totals)
}

fn run(args: &[String]) -> Result<Option<Totals>, (u8, String)> {
    let usage = || (2, "usage: gogx INSTALLER DIR | gogx --test INSTALLER | gogx --version".to_string());
    let (installer, out) = match args {
        [flag] if flag == "--version" => {
            println!("gogx {} (innospect 0.2.0 with Arcanum CE's patches)", env!("CARGO_PKG_VERSION"));
            return Ok(None);
        }
        [flag, installer] if flag == "--test" => (installer, Output::Test),
        [installer, dir] if !installer.starts_with('-') && !dir.starts_with('-') => {
            let dir = PathBuf::from(dir);
            if dir.exists() && fs::read_dir(&dir).map_err(|e| (1, format!("{}: {e}", dir.display())))?.next().is_some() {
                return Err((1, format!("{} is not empty", dir.display())));
            }
            (installer, Output::Dir(dir))
        }
        _ => return Err(usage()),
    };
    let file = File::open(installer).map_err(|e| (1, format!("{installer}: {e}")))?;
    // Safety: the map is read-only, and the installer is the player's own
    // download, not a file anything else writes while gogx runs.
    let map = unsafe { memmap2::Mmap::map(&file) }.map_err(|e| (1, format!("{installer}: {e}")))?;
    let inst = InnoInstaller::from_bytes(&map)
        .map_err(|e| (1, format!("{installer} is not an installer gogx can read: {}", describe(&e))))?;
    extract(&inst, &out).map(Some).map_err(|e| (1, e))
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match run(&args) {
        Ok(None) => ExitCode::SUCCESS,
        Ok(Some(totals)) => {
            println!("gogx: {} files, {} MB, all checked", totals.files, totals.bytes / 1_000_000);
            ExitCode::SUCCESS
        }
        Err((code, message)) => {
            eprintln!("gogx: {message}");
            ExitCode::from(code)
        }
    }
}
