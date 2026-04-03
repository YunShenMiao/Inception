#!/bin/sh

printf "Content-Type: text/plain; charset=utf-8\r\n"
printf "\r\n"
printf "Shell CGI demo at %s\n" "$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
printf "SCRIPT_NAME=%s\n" "${SCRIPT_NAME}"
printf "PATH_INFO=%s\n" "${PATH_INFO}"
printf "QUERY_STRING=%s\n" "${QUERY_STRING}"
printf "REMOTE_ADDR=%s\n" "${REMOTE_ADDR}"
printf "Note: this script omits Content-Length so CatSurf will stream until EOF.\n"
