use clap::Parser;
use std::io::Read;
use std::path::Path;
use std::time::{Duration, Instant};
use std::{process::Command, str::FromStr};

#[derive(Debug)]
struct Submission {
    language: String,
    name: String,
}

impl Submission {
    fn path(&self) -> String {
        Path::new("submissions")
            .join(&self.language)
            .join(&self.name)
            .to_str()
            .unwrap()
            .to_string()
    }
}

impl FromStr for Submission {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut parts = s.split_whitespace();
        let language = parts.next().ok_or("No language specified")?;
        let name = parts.next().ok_or("No name specified")?;
        Ok(Submission {
            language: language.to_string(),
            name: name.to_string(),
        })
    }
}

#[derive(Parser, Debug)]
#[clap(about, version, author)]
struct Args {
    #[clap(short, long)]
    first: Submission,
    #[clap(short, long)]
    second: Submission,
}

fn launch_submission(path: String) -> std::process::Child {
    Command::new("make")
        .arg("-C")
        .arg(path)
        .arg("output")
        .stdout(std::process::Stdio::piped())
        .spawn()
        .expect("failed to execute process")
}

const BUFFER_SIZE: usize = (1024 * 1024) as usize;

fn seek_to_fizz_start<T: Read>(reader: &mut T) -> Result<(), std::io::Error> {
    let mut buffer = [0; 1];

    let mut state = 0;

    loop {
        let n = reader.read(&mut buffer).unwrap();

        if n == 0 {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "Fizz not found",
            ));
        }

        match state {
            0 => {
                if buffer[0] == b'\n' {
                    state = 1;
                }
            }
            1 => {
                if buffer[0] == b'1' {
                    state = 2;
                } else {
                    state = 0;
                }
            }
            2 => {
                if buffer[0] == b'\n' {
                    return Ok(());
                } else {
                    state = 0;
                }
            }
            _ => unreachable!(),
        }
    }
}

const GIB: f64 = 1024.0 / 1024.0 / 1024.0;

fn main() {
    let args = Args::parse();

    let mut first_submission = launch_submission(args.first.path());
    let mut second_submission = launch_submission(args.second.path());

    let mut stdout_first = first_submission.stdout.take().unwrap();
    let mut stdout_second = second_submission.stdout.take().unwrap();

    // Get rid of junk in the beginning of the output by seeking to the first "\n1\n"
    seek_to_fizz_start(&mut stdout_first).unwrap();
    seek_to_fizz_start(&mut stdout_second).unwrap();

    let mut progress = 0 as usize;
    let mut last_progress = 0 as usize;
    let mut last_update = Instant::now();

    loop {
        unsafe {
            static mut BUF1: [u8; BUFFER_SIZE] = [0; BUFFER_SIZE];
            static mut BUF2: [u8; BUFFER_SIZE] = [0; BUFFER_SIZE];

            stdout_first.read_exact(&mut BUF1).unwrap();
            stdout_second.read_exact(&mut BUF2).unwrap();

            let slice1 = &BUF1[..BUFFER_SIZE];
            let slice2 = &BUF2[..BUFFER_SIZE];

            if slice1 != slice2 {
                let s1 = String::from_utf8(slice1.to_vec()).unwrap();
                let s2 = String::from_utf8(slice2.to_vec()).unwrap();

                println!("{}\n======\n{}", s1, s2);
                dbg!(slice1);
                dbg!(slice2);
                break;
            }

            progress += BUFFER_SIZE;
        }

        if Instant::now() - last_update > Duration::from_secs(1) {
            println!(
                "Verified {:.2} GiB at {:.2} GiB/s",
                (progress as f64) / GIB,
                (progress - last_progress) as f64 / GIB
            );
            last_update = Instant::now();
            last_progress = progress;
        }
    }

    first_submission.kill().unwrap();
    first_submission.wait().unwrap();

    second_submission.kill().unwrap();
    second_submission.wait().unwrap();
}
