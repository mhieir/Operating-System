#!/bin/bash
# Usage: bash benchmark.sh script_to_run.sh output_file

SCRIPT="$1"
OUTFILE="$2"
RUNS=10

TMPFILE=$(mktemp)

> "$OUTFILE"

echo "Running $SCRIPT $RUNS times and collecting statistics..."

for i in $(seq 1 $RUNS); do
    echo "Run $i..."
    # Capture /usr/bin/time -v output
    TIME_OUTPUT=$( /usr/bin/time -v bash "$SCRIPT" 2>&1 )

    # Extract relevant fields using grep and awk
    COMMAND=$(echo "$TIME_OUTPUT" | grep 'Command being timed:' | awk -F'"' '{print $2}')
    USER_TIME=$(echo "$TIME_OUTPUT" | grep 'User time (seconds):' | awk '{print $4}')
    SYS_TIME=$(echo "$TIME_OUTPUT" | grep 'System time (seconds):' | awk '{print $4}')
    CPU_PERC=$(echo "$TIME_OUTPUT" | grep 'Percent of CPU this job got:' | awk '{print $7}')
    ELAPSED=$(echo "$TIME_OUTPUT" | grep 'Elapsed (wall clock) time' | awk '{print $8}')

    # Convert elapsed time to seconds
    if [[ $ELAPSED == *:*:* ]]; then
        # h:mm:ss
        H=$(echo $ELAPSED | cut -d: -f1)
        M=$(echo $ELAPSED | cut -d: -f2)
        S=$(echo $ELAPSED | cut -d: -f3)
        ELAPSED_SEC=$(echo "$H*3600 + $M*60 + $S" | bc)
    elif [[ $ELAPSED == *:* ]]; then
        # m:ss
        M=$(echo $ELAPSED | cut -d: -f1)
        S=$(echo $ELAPSED | cut -d: -f2)
        ELAPSED_SEC=$(echo "$M*60 + $S" | bc)
    else
        ELAPSED_SEC=$ELAPSED
    fi

    FS_INPUT=$(echo "$TIME_OUTPUT" | grep 'File system inputs' | awk '{print $5}')
    FS_OUTPUT=$(echo "$TIME_OUTPUT" | grep 'File system outputs' | awk '{print $5}')

    # Append values as a space-separated line
    echo "$USER_TIME $SYS_TIME $CPU_PERC $ELAPSED_SEC $FS_INPUT $FS_OUTPUT" >> "$TMPFILE"
done

awk '
{
    user+=$1; sys+=$2; cpu+=$3; elapsed+=$4; fsi+=$5; fso+=$6
}
END {
    n=NR
    printf "Command: %s\n", "'"$SCRIPT"'"
    printf "Average User time (seconds): %.3f\n", user/n
    printf "Average System time (seconds): %.3f\n", sys/n
    printf "Average CPU %%: %.2f\n", cpu/n
    printf "Average Elapsed time (seconds): %.3f\n", elapsed/n
    printf "Average File system inputs: %.3f\n", fsi/n
    printf "Average File system outputs: %.3f\n", fso/n
}' "$TMPFILE" > "$OUTFILE"

echo "Averages written to $OUTFILE"

rm "$TMPFILE"

