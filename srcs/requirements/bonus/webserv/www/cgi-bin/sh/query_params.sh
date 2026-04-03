#!/bin/sh

qs="${QUERY_STRING}"
printf "Status: 200 OK\r\n"
printf "Content-Type: text/plain; charset=utf-8\r\n\r\n"
printf "Shell CGI query parser demo\n"
printf "Raw QUERY_STRING: %s\n\n" "${qs:- (empty)}"

if [ -z "$qs" ]; then
    printf "(no query parameters provided)\n"
    exit 0
fi

IFS='&' read -ra pairs <<EOF
$qs
EOF

for kv in "${pairs[@]}"; do
    key=${kv%%=*}
    value=${kv#*=}
    printf "%s = %s\n" "$key" "$value"
done
