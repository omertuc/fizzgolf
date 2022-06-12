use nix::unistd::write;
use std::time::{Duration, Instant};

use libc::exit;
use itoap;

#[inline(always)]
fn u64_to_bytes(n: u64, v: &mut Vec<u8>) {
    itoap::write_to_vec(v, n);
    v.push(0x0a);
}

fn main() {
    let mut i: u64 = 0;

    let Fizz = "Fizz\n".as_bytes();
    let Buzz = "Buzz\n".as_bytes();
    let FizzBuzz = "FizzBuzz\n".as_bytes();

    let mut buffering: Vec<u8> = Vec::with_capacity(1024*1024*2);
    loop {
        for _ in 0..512 {
            i += 1;
            u64_to_bytes(i, &mut buffering);
        
            i += 1;
            u64_to_bytes(i, &mut buffering);

            i += 1;
            buffering.extend_from_slice(&Fizz);

            i += 1;
            u64_to_bytes(i, &mut buffering);

            i += 1;
            buffering.extend_from_slice(&Buzz);

            i += 1;
            buffering.extend_from_slice(&Fizz);

            i += 1;
            u64_to_bytes(i, &mut buffering);

            i += 1;
            u64_to_bytes(i, &mut buffering);

            i += 1;
            buffering.extend_from_slice(&Fizz);

            i += 1;
            buffering.extend_from_slice(&Buzz);

            i += 1;
            u64_to_bytes(i, &mut buffering);

            i += 1;
            buffering.extend_from_slice(&Fizz);

            i += 1;
            u64_to_bytes(i, &mut buffering);

            i += 1;
            u64_to_bytes(i, &mut buffering);

            i += 1;
            buffering.extend_from_slice(&FizzBuzz);            
        }
        write(1, buffering.as_slice());
        buffering.clear();
    }
}
