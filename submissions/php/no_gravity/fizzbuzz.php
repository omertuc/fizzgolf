#!/bin/php
<?php

ob_start();

for ($i = 0; $i < PHP_INT_MAX; $i++) {
        $out = $i;
        if ($i % 3 === 0) $out = 'Fizz';
        if ($i % 5 === 0) {
                $out = 'Buzz';
                if ($i % 3 === 0) $out = 'FizzBuzz';
        }
        echo "$out\n";
        if ($i % 8192 === 0) ob_flush();
}
