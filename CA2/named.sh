for i in $(seq 1 1000); do
  mkfifo /tmp/f
  (cat /tmp/f > /dev/null &) 
  dd if=/dev/zero bs=1M count=1 of=/tmp/f status=none
  rm /tmp/f
done

