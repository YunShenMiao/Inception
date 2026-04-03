#!/usr/bin/php
<?php
 $body = file_get_contents("php://input");

header('Status: 200 OK');
header("Content-Type: text/plain; charset=utf-8");

echo "PHP CGI upload demo\n";
echo "REQUEST_METHOD=" . ($_SERVER["REQUEST_METHOD"] ?? "") . "\n";
echo "CONTENT_TYPE=" . ($_SERVER["CONTENT_TYPE"] ?? "") . "\n";
echo "CONTENT_LENGTH=" . ($_SERVER["CONTENT_LENGTH"] ?? "") . "\n";
echo "SCRIPT_NAME=" . ($_SERVER["SCRIPT_NAME"] ?? "") . "\n";
echo "PATH_INFO=" . ($_SERVER["PATH_INFO"] ?? "") . "\n";
echo "QUERY_STRING=" . ($_SERVER["QUERY_STRING"] ?? "") . "\n";
echo "REMOTE_ADDR=" . ($_SERVER["REMOTE_ADDR"] ?? "") . ":" . ($_SERVER["REMOTE_PORT"] ?? "") . "\n";
echo "\n";
echo "First 256 bytes of body:\n";
echo substr($body, 0, 256);
if (strlen($body) > 256) {
    echo "\n... (" . (strlen($body) - 256) . " more bytes)";
}
echo "\n";

echo "\nUploaded key/value pairs:\n";
foreach ($_POST as $key => $value) {
    echo $key . "=" . $value . "\n";
}

echo "\nHTTP headers:\n";
foreach ($_SERVER as $key => $value) {
    if (strpos($key, "HTTP_") === 0) {
        echo $key . "=" . $value . "\n";
    }
}
