<?php
header("Content-Type: text/plain");
echo "Olá do PHP-CGI!\n";
echo "Method: " . $_SERVER['REQUEST_METHOD'] . "\n";
echo "Query: " . $_SERVER['QUERY_STRING'] . "\n";
echo "Path Info: " . $_SERVER['PATH_INFO'] . "\n";
echo "Remote Addr: " . $_SERVER['REMOTE_ADDR'] . "\n";
?>