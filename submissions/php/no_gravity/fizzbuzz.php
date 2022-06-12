#!/bin/php
<?php

ob_start(null, 32000);

$a = [0, 1, 'Fizz', 3, 'Buzz', 'Fizz',
      6, 7, 'Fizz', 'Buzz', 10, 'Fizz',
      12, 13, 'FizzBuzz', ''];

for ($i = 1; $i < PHP_INT_MAX-15; $i+=15) {
    $a[0] = $i;
    $a[1] = $i+1;
    $a[3] = $i+3;
    $a[6] = $i+6;
    $a[7] = $i+7;
    $a[10] = $i+10;
    $a[12] = $i+12;
    $a[13] = $i+13;
    echo join("\n", $a);
}
