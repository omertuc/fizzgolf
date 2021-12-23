use clap::Parser;
use std::io::Read;
use std::process::Command;
use std::time::{Duration, Instant};

fn launch_submission(path: String) -> std::process::Child {
    Command::new("make")
        .arg("-C")
        .arg(path)
        .arg("output")
        .stdout(std::process::Stdio::piped())
        .spawn()
        .expect("failed to execute process")
}

#[derive(Parser, Debug)]
#[clap(about, version, author)]
struct Args {
    first: String,
    second: String,
}

const BUFFER_SIZE: usize = (1024 * 1024) as usize;

fn seek_to_fizz_start<T: Read>(reader: &mut T) -> Result<(), std::io::Error> {
    let mut buffer = [0; 1];
    let mut state = 0;

    loop {
        reader.read_exact(&mut buffer)?;
        state = match state {
            0 if buffer[0] == b'\n' => 1,
            0 => 0,
            1 if buffer[0] == b'1' => 2,
            1 => 0,
            2 if buffer[0] == b'\n' => return Ok(()),
            2 => 0,
            _ => unreachable!(),
        }
    }
}

const GIB: f64 = 1024.0 * 1024.0 * 1024.0;

fn main() {
    let args = Args::parse();

    let mut first_submission = launch_submission(args.first);
    let mut second_submission = launch_submission(args.second);

    let mut stdout_first = first_submission.stdout.take().unwrap();
    let mut stdout_second = second_submission.stdout.take().unwrap();

    // Get rid of junk in the beginning of the output by seeking to the first "\n1\n"
    seek_to_fizz_start(&mut stdout_first).unwrap();
    seek_to_fizz_start(&mut stdout_second).unwrap();

    let mut progress = 0 as usize;
    let mut last_progress = 0 as usize;
    let mut last_update = Instant::now();

    static mut BUF1: [u8; BUFFER_SIZE] = [0; BUFFER_SIZE];
    static mut BUF2: [u8; BUFFER_SIZE] = [0; BUFFER_SIZE];

    loop {
        unsafe {
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
            let progress_gib = (progress as f64) / GIB;
            let rate_gib = (progress - last_progress) as f64 / GIB;
            println!("Verified {:.2} GiB at {:.2} GiB/s", progress_gib, rate_gib);
            last_update = Instant::now();
            last_progress = progress;
        }
    }

    first_submission.kill().unwrap();
    first_submission.wait().unwrap();

    second_submission.kill().unwrap();
    second_submission.wait().unwrap();
}
