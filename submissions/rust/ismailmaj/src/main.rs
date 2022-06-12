use itoap;
use nix::unistd::write;

const FIZZ_U8: *const u8 = [70u8, 105, 122, 122, 10].as_ptr();

const BUZZ_U8: *const u8 = [66, 117, 122, 122, 10].as_ptr();

const FIZZBUZZ_U8: *const u8 = [70, 105, 122, 122, 66, 117, 122, 122, 10].as_ptr();

const MULTIPLIER: usize = 8;
const BUFFER_SIZE: usize = 8192 * MULTIPLIER;
const LOOP_SIZE: usize = 40 * MULTIPLIER;
const NEW_LINE: u8 = 10;

#[allow(unused_must_use)]
fn main() {
    let mut BUFFER = vec![0u8; BUFFER_SIZE];
    let mut offset: usize = 0;
    unsafe {
        loop {
            let mut pointer: *mut u8 = BUFFER.as_mut_ptr();
            (0..LOOP_SIZE).for_each(|_| {
                pointer.copy_from_nonoverlapping(FIZZBUZZ_U8, 9);
                pointer = pointer.add(9);

                pointer = pointer.add(itoap::write_to_ptr(pointer, offset + 1));
                pointer.write(NEW_LINE);
                pointer = pointer.add(1);

                pointer = pointer.add(itoap::write_to_ptr(pointer, offset + 2));
                pointer.write(NEW_LINE);
                pointer = pointer.add(1);

                pointer.copy_from_nonoverlapping(FIZZ_U8, 5);
                pointer = pointer.add(5);

                pointer = pointer.add(itoap::write_to_ptr(pointer, offset + 4));
                pointer.write(NEW_LINE);
                pointer = pointer.add(1);

                pointer.copy_from_nonoverlapping(BUZZ_U8, 5);
                pointer = pointer.add(5);

                pointer.copy_from_nonoverlapping(FIZZ_U8, 5);
                pointer = pointer.add(5);

                pointer = pointer.add(itoap::write_to_ptr(pointer, offset + 7));
                pointer.write(NEW_LINE);
                pointer = pointer.add(1);

                pointer = pointer.add(itoap::write_to_ptr(pointer, offset + 8));
                pointer.write(NEW_LINE);
                pointer = pointer.add(1);

                pointer.copy_from_nonoverlapping(FIZZ_U8, 5);
                pointer = pointer.add(5);

                pointer.copy_from_nonoverlapping(BUZZ_U8, 5);
                pointer = pointer.add(5);

                pointer = pointer.add(itoap::write_to_ptr(pointer, offset + 11));
                pointer.write(NEW_LINE);
                pointer = pointer.add(1);

                pointer.copy_from_nonoverlapping(FIZZ_U8, 5);
                pointer = pointer.add(5);

                pointer = pointer.add(itoap::write_to_ptr(pointer, offset + 13));
                pointer.write(NEW_LINE);
                pointer = pointer.add(1);

                pointer = pointer.add(itoap::write_to_ptr(pointer, offset + 14));
                pointer.write(NEW_LINE);
                pointer = pointer.add(1);

                offset += 15;
            });
            write(1, BUFFER.as_slice());
        }
    }
}
