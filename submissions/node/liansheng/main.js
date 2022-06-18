let buffer = "";

for (i = 0; i < 1e19; i += 15) {
    buffer += `${i+1}\n${i+2}\nFizz\n${i+4}\nBuzz\nFizz\n${i+7}\n${i+8}\nFizz\nBuzz\n${i+11}\nFizz\n${i+13}\n${i+14}\nFizzBuzz\n`;

    if(buffer.length > 40000) {
        process.stdout.write(buffer);
        buffer = "";
    }
}

process.stdout.write(buffer);
