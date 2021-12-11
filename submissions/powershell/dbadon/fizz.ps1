$i = 1
#While ($i -gt 0){
While ($i -lt 100000){    # Limiting to 100k for testing
if ((($i % 5) -eq 0) -and (($i % 3) -eq 0)){Write-Host "FizzBuzz"}
elseif (($i % 3) -eq 0) {Write-Host "Fizz"}
elseif (((($i -split "" | Select-Object -SkipLast 1) | 
    Select-Object -Last 1) - eq 0) -or ((($i -split "" | 
    Select-Object -SkipLast 1) | Select-Object -Last 1) -eq 5)){Write-Host "Buzz"}
else {Write-Host $i}
$i++}
