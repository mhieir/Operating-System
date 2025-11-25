for i in $(seq 1 1000); do
  dd if=/dev/zero bs=1M count=1 status=none | cat > /dev/null
done

