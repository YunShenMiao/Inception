#!/usr/bin/php
<?php
parse_str($_SERVER['QUERY_STRING'] ?? '', $params);

header('Status: 200 OK');
header('Content-Type: text/plain; charset=utf-8');
echo "PHP CGI query parser demo\n";
echo 'Raw QUERY_STRING: ' . (($_SERVER['QUERY_STRING'] ?? '') ?: '(empty)') . "\n\n";

if (empty($params)) {
    echo "(no query parameters provided)\n";
} else {
    echo "Decoded parameters:\n";
    foreach ($params as $key => $value) {
        if (is_array($value)) {
            $value = implode(', ', $value);
        }
        echo $key . ' = ' . $value . "\n";
    }
}
?>
