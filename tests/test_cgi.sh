#!/bin/bash
# simple CGI smoke tests
SERVER=localhost:8080

echo "GET Python CGI"
curl -s http://$SERVER/cgi-bin/hello.py

echo

echo "GET PHP CGI"
curl -s http://$SERVER/cgi-bin/hello.php
