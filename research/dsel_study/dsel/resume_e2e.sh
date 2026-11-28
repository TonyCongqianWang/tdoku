#!/bin/bash
# Resume the end-to-end timing study (about 40 min). Check the machine is idle first (uptime).
# Three processes in sequence on a P-core (cpu 2), one in parallel on an E-core (cpu 9).
cd "$(dirname "$0")"
D=$(pwd)
rm -f e2e_p*.txt e2e_e1.txt
(./run_e2e.sh 2 $D/e2e_p1.txt 1; ./run_e2e.sh 2 $D/e2e_p2.txt 2; ./run_e2e.sh 2 $D/e2e_p3.txt 3) &
(./run_e2e.sh 9 $D/e2e_e1.txt 1) &
wait
grep -c "" e2e_p1.txt e2e_p2.txt e2e_p3.txt e2e_e1.txt
grep -h MISMATCH e2e_*.txt | head
