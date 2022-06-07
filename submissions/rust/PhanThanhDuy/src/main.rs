use nix::unistd::write;
use std::time::{Duration, Instant};

use libc::exit;
use itoap;

#[inline(always)]
fn u32_to_bytes(n: u32, v: &mut Vec<u8>) {
    itoap::write_to_vec(v, n);
    v.push(0x0a);
}

fn main() {
    let mut i: u32 = 1;

    let Fizz = "Fizz\n".as_bytes();
    let Buzz = "Buzz\n".as_bytes();
    let FizzBuzz = "FizzBuzz\n".as_bytes();

    let mut buffering: Vec<u8> = Vec::with_capacity(1024*1024*2);
    loop {
        for _ in 0..32 {
            // total_bytes += nlength(i) as u64;
            u32_to_bytes(i, &mut buffering);
        
            // total_bytes += nlength(i+1) as u64;
            u32_to_bytes(i+1, &mut buffering);

            // i+2
            // total_bytes += 5;
            buffering.extend_from_slice(&Fizz);

            // i+3
            // total_bytes += nlength(i+3) as u64;
            u32_to_bytes(i+3, &mut buffering);

            // i+4
            // total_bytes += 5;
            buffering.extend_from_slice(&Buzz);

            // i+5
            // total_bytes += 5;
            buffering.extend_from_slice(&Fizz);

            // i+6
            // total_bytes += nlength(i+6) as u64;
            u32_to_bytes(i+6, &mut buffering);

            // i+7
            // total_bytes += nlength(i+7) as u64;
            u32_to_bytes(i+7, &mut buffering);

            // i+8
            // total_bytes += 5;
            buffering.extend_from_slice(&Fizz);

            // i+9
            // total_bytes += 5;
            buffering.extend_from_slice(&Buzz);

            // i+10
            // total_bytes += nlength(i+10) as u64;
            u32_to_bytes(i+10, &mut buffering);

            // i+11
            // total_bytes += 5;
            buffering.extend_from_slice(&Fizz);

            // i+12
            // total_bytes += nlength(i+12) as u64;
            u32_to_bytes(i+12, &mut buffering);

            // i+13
            // total_bytes += nlength(i+13) as u64;
            u32_to_bytes(i+13, &mut buffering);

            // i+14
            // total_bytes += 9;
            buffering.extend_from_slice(&FizzBuzz);            
            i += 15;
        }
        write(1, buffering.as_slice());
        buffering.clear();
    }
}
