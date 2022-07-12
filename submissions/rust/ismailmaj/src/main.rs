use crossbeam::scope;
use itoap;
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering;
use std::sync::Arc;
use nix::unistd::write;


const FIZZ_U8: *const u8 = b"Fizz\n".as_ptr();

const BUZZ_U8: *const u8 =  b"Buzz\n".as_ptr();

const FIZZBUZZ_U8: *const u8 =  b"FizzBuzz\n".as_ptr();

const NEW_LINE: u8 = b"\n"[0];

const MULTIPLIER: usize = 18; // tunable parameter

const BUFFER_SIZE: usize = 4192 * MULTIPLIER;

const LOOP_SIZE: usize = 15;
const LOOP_COUNT: usize = 20 * MULTIPLIER;
const OFFSET_GAP: usize = LOOP_SIZE * LOOP_COUNT;

const THREAD_COUNT: usize = 15;

fn main() {
    let shared_counter = Arc::new(AtomicUsize::new(0));
    scope(|scope| {
        (0..THREAD_COUNT).for_each(|thread_id| {
            let cloned_shared_counter = shared_counter.clone();
            scope.spawn(move |_| {
                task(thread_id, cloned_shared_counter);
            });
        });
    });
}

fn task(thread_id: usize, counter: Arc<AtomicUsize>) {
    let mut counter_offset = thread_id;
    let mut buffer = vec![0u8; BUFFER_SIZE];
    let mut offset = thread_id * OFFSET_GAP;
    loop {
        let mut pointer = buffer.as_mut_ptr();
        (0..LOOP_COUNT).for_each(|_| {
            unsafe { pointer = do_work(pointer, offset) };
            offset += LOOP_SIZE;
        });
        while counter.load(Ordering::SeqCst) != counter_offset {}
        write(1, &buffer[..pointer as usize - buffer.as_ptr() as usize]);
        while counter
            .compare_exchange(
                counter_offset,
                counter_offset + 1,
                Ordering::SeqCst,
                Ordering::SeqCst,
            )
            .is_err()
        {}
        counter_offset += THREAD_COUNT;
        offset += OFFSET_GAP * (THREAD_COUNT - 1);
    }
}

#[inline(always)]
unsafe fn do_work(mut pointer: *mut u8, offset: usize) -> *mut u8 {
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
    return pointer;
}
