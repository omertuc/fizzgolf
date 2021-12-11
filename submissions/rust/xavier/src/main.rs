use std::error::Error;
use std::fmt::Write;
use std::io::Write as OtherWrite;

const LEN: usize = 1000000000;

fn main() -> Result<(), Box<dyn Error>> {
    let stdout = std::io::stdout();
    let mut stdout = std::io::BufWriter::new(stdout.lock());
    let mut buffer = String::new();
    buffer.reserve(80);
    let mut n = 0usize;
    while n < LEN {
        write!(
            &mut buffer,
            r#"{}
{}
Fizz
{}
Buzz
Fizz
{}
{}
Fizz
Buzz
{}
Fizz
{}
{}
FizzBuzz
"#,
            n + 1,
            n + 2,
            n + 4,
            n + 7,
            n + 8,
            n + 11,
            n + 13,
            n + 14
        )?;
        stdout.write_all(buffer.as_bytes())?;
        n += 15;
        buffer.clear(); // forgot that ...
    }
    Ok(())
}
