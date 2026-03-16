#!/bin/bash
# test_nonblocking.sh

echo "=== Teste 1: pedido normal ==="
curl -s http://127.0.0.1:8080/cgi-bin/hello.php > /dev/null && echo "OK" || echo "FALHOU"

echo "=== Teste 2: CGI lento não bloqueia outros ==="
curl -s http://127.0.0.1:8080/cgi-bin/slow.php &
SLOW_PID=$!
sleep 0.2

START=$(date +%s%N)
curl -s http://127.0.0.1:8080/cgi-bin/hello.php > /dev/null
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))

if [ $ELAPSED -lt 1000 ]; then
    echo "OK — respondeu em ${ELAPSED}ms enquanto slow.php corria"
else
    echo "BLOQUEANTE — demorou ${ELAPSED}ms (esperava < 1000ms)"
fi

wait $SLOW_PID

echo "=== Teste 3: 10 pedidos simultâneos ==="
for i in $(seq 1 10); do
    curl -s http://127.0.0.1:8080/cgi-bin/hello.php > /dev/null &
done
wait && echo "OK — todos completaram"