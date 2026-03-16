#!/usr/bin/env python3
# slow_client.py
import socket
import time

s = socket.socket()
s.connect(('127.0.0.1', 8080))

# Envia headers byte a byte com delay
request = "GET /cgi-bin/hello.php HTTP/1.1\r\nHost: localhost\r\n\r\n"
for char in request:
    s.send(char.encode())
    time.sleep(0.1)  # 100ms entre cada byte

response = s.recv(4096)
print(response.decode())
s.close()