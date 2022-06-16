<?php

$nWorkers = preg_match_all('/^processor\s/m', file_get_contents('/proc/cpuinfo'), $discard);
$iterationsPerFlush = 20000;
define('CHILD_TOKEN_KEY', 0);

//1 byte SHM segment holding the worker ID whose turn it is to write 
//initialise to child 0.
$shm = shm_attach(1);
shm_put_var($shm, CHILD_TOKEN_KEY, 0);

//Semaphore to protect access to the SHM var
$key = ftok(__FILE__, 1);
$sem = sem_get($key);
$childPids = [];
pcntl_sigprocmask(SIG_BLOCK, [SIGTERM, SIGINT, SIGCHLD]);

for ($i = 0; $i < $nWorkers; $i++) {
    if (!$pid = pcntl_fork()) {
        doWork($sem, $shm, $i, $nWorkers, $iterationsPerFlush);
        exit;
    } else {
        $childPids[] = $pid;
    }
}

//If we get killed or one of our children does, clean everything up
pcntl_sigwaitinfo([SIGTERM, SIGINT, SIGCHLD], $info);
foreach ($childPids as $pid) {
    posix_kill($pid, SIGKILL);
}
sem_remove($sem);
shm_remove($shm);

function doWork($sem, $shm, $childId, $nWorkers, $iterationsPerFlush)
{
    $nextChildId = ($childId + 1) % $nWorkers;
    $increment = ($nWorkers - 1) * $iterationsPerFlush * 15;
    $startOffset = $childId * $iterationsPerFlush * 15;

    $a = [
        -14 + $startOffset, -13 + $startOffset, "fizz", -11 + $startOffset, "buzz\nfizz",
        -8 + $startOffset, -7 + $startOffset, "fizz\nbuzz", -4 + $startOffset, "fizz",
        -2 + $startOffset, -1 + $startOffset, "fizzbuzz\n"
    ];

    ob_start();

    while (true) {
        for ($i = 0; $i < $iterationsPerFlush; $i++) {
            $a[0] += 15;
            $a[1] += 15;
            $a[3] += 15;
            $a[5] += 15;
            $a[6] += 15;
            $a[8] += 15;
            $a[10] += 15;
            $a[11] += 15;
            echo join("\n", $a);
        }

        //Acquire the semaphore and check if it's our turn to flush.  If not, repeat.
        while (true) {
            sem_acquire($sem);
            if (shm_get_var($shm, CHILD_TOKEN_KEY) === $childId) {
                break;
            }
            sem_release($sem);
        }

        ob_flush();

        //Pass the token on to the next child and release the semaphore
        shm_put_var($shm, CHILD_TOKEN_KEY, $nextChildId);
        sem_release($sem);

        //Skip over the numbers the other workers already handled
        $a[0] += $increment;
        $a[1] += $increment;
        $a[3] += $increment;
        $a[5] += $increment;
        $a[6] += $increment;
        $a[8] += $increment;
        $a[10] += $increment;
        $a[11] += $increment;
    }
}
