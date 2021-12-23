use clap::Parser;
use std::io::{Read, Write};
use std::process::Command;
use std::time::{Duration, Instant};

#[derive(Parser, Debug)]
struct Args {
    submissions: Vec<String>,
}

fn seek_to_fizz_start<T: Read>(reader: &mut T) -> Result<(), std::io::Error> {
    let mut buffer = [0; 1];
    let mut state = 0;

    loop {
        reader.read_exact(&mut buffer)?;
        state = match state {
            0 if buffer[0] == b'\n' => 1,
            1 if buffer[0] == b'1' => 2,
            2 if buffer[0] == b'\n' => return Ok(()),
            _ => 0,
        }
    }
}

const GIB: f64 = 1024.0 * 1024.0 * 1024.0;
const BUFFER_SIZE: usize = 64 * 1024;

fn main() {
    let submissions = Args::parse().submissions;

    let mut childs = submissions
        .iter()
        .map(|path| {
            Command::new("make")
                .arg("-C")
                .arg(path)
                .arg("output")
                .stdout(std::process::Stdio::piped())
                .spawn()
        })
        .collect::<Result<Vec<_>, _>>()
        .unwrap();

    let mut stdouts = childs
        .iter_mut()
        .map(|child| child.stdout.take().unwrap())
        .collect::<Vec<_>>();

    stdouts
        .iter_mut()
        .try_for_each(|stdout| seek_to_fizz_start(stdout))
        .unwrap();

    let mut buffers = (0..stdouts.len())
        .map(|_| [0; BUFFER_SIZE])
        .collect::<Vec<_>>();

    let mut progress = 0 as usize;
    let mut last_progress = 0 as usize;
    let mut last_update = Instant::now();

    loop {
        stdouts
            .iter_mut()
            .zip(buffers.iter_mut())
            .try_for_each(|(stdout, buffer)| stdout.read_exact(buffer))
            .unwrap();

        if !buffers.iter().all(|buffer| *buffer == buffers[0]) {
            println!("Not all submissions match! Dumping current buffer to files, run `sha256sum buffer-*.bin` to check.");
            for (buffer, submission) in buffers.iter().zip(submissions) {
                std::fs::File::create(format!("buffer-{}.bin", submission.replace("/", "-")))
                    .unwrap()
                    .write_all(buffer)
                    .unwrap();
            }
            break;
        }

        progress += BUFFER_SIZE;

        if Instant::now() - last_update > Duration::from_secs(1) {
            let progress_gib = (progress as f64) / GIB;
            let rate_gib = (progress - last_progress) as f64 / GIB;
            println!("Verified {:.2} GiB at {:.2} GiB/s", progress_gib, rate_gib);
            last_update = Instant::now();
            last_progress = progress;
        }
    }

    childs.iter_mut().for_each(|sub| sub.kill().unwrap());
}
