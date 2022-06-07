#!/bin/php
<?php

ob_start();

for ($i = 0; $i < 100000000; $i++) {
        $out = $i;
        if ($i % 3 === 0) $out = 'fizz';
        if ($i % 5 === 0) {
                $out = 'buzz';
                if ($i % 3 === 0) $out = 'fizzbuzz';
        }
        echo "$out\n";
        if ($i % 8192 === 0) ob_flush();
}
