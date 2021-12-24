use clap::Parser;
use std::io::{Read, Write};
use std::process::{Child, Command};
use std::time::{Duration, Instant};
use derive_deref::{Deref, DerefMut};

const KIB: usize = 1024;
const MIB: usize = KIB * 1024;
const GIB: usize = MIB * 1024;

const BUFFER_SIZE: usize = 64 * KIB;

#[derive(Parser, Debug)]
struct Args {
    submissions: Vec<String>,
}

fn seek_to_fizz_start<T: Read>(reader: &mut T) -> Result<(), std::io::Error> {
    let mut buffer = [0; 1];
    let mut state = 0;
    let mut skipped = 0;

    while skipped < 300 * KIB {
        reader.read_exact(&mut buffer)?;
        skipped += 1;

        state = match state {
            0 if buffer[0] == b'\n' => 1,
            1 if buffer[0] == b'1' => 2,
            2 if buffer[0] == b'\n' => return Ok(()),
            _ => 0,
        }
    }

    Err(std::io::Error::new(
        std::io::ErrorKind::InvalidData,
        "FizzBuzz start (newline followed by the number 1 followed by another newline) not found after 300 KiB of output, please revise your submission",
    ))
}

#[derive(Deref, DerefMut)]
struct DroppableChild(Child);

impl Drop for DroppableChild {
    fn drop(&mut self) {
        self.0.kill().unwrap();
    }
}

impl From<Child> for DroppableChild {
    fn from(child: Child) -> Self {
        DroppableChild(child)
    }
}

fn main() {
    let submissions = Args::parse().submissions;

    let mut childs: Vec<DroppableChild> = submissions
        .iter()
        .map(|path| {
            Command::new("make")
                .arg("-C")
                .arg(path)
                .arg("output")
                .stdout(std::process::Stdio::piped())
                .spawn()
                .expect("Failed to spawn make")
        })
        .map(DroppableChild::from)
        .collect();

    let mut stdouts = childs
        .iter_mut()
        .map(|child| child.stdout.take().unwrap())
        .collect::<Vec<_>>();

    stdouts
        .iter_mut()
        .try_for_each(|stdout| seek_to_fizz_start(stdout))
        .expect("FizzBuzz start not found");

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
            let progress_gib = (progress as f64) / GIB as f64;
            let rate_gib = (progress - last_progress) as f64 / GIB as f64;
            println!("Verified {:.2} GiB at {:.2} GiB/s", progress_gib, rate_gib);
            last_update = Instant::now();
            last_progress = progress;
        }
    }
}
