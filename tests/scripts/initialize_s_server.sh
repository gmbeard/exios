#!/usr/bin/env bash

tmpdir="${TEST_TMPDIR:-/tmp}"
pidfile="${tmpdir}/s_server.pid"

openssl req \
    -x509 \
    -newkey rsa:2048 \
    -keyout "${tmpdir}/key.pem" \
    -out "${tmpdir}/cert.pem" \
    -days 365 \
    -nodes <<-EOF
GB
Gtr. Manchester
Manchester
Exios
Software
localhost
no.spam@please.com
EOF

openssl s_server \
    -key "${tmpdir}/key.pem" \
    -cert "${tmpdir}/cert.pem" \
    -accept 8443 \
    -4 \
    -www >/dev/null 2>&1 &

echo $! > "${pidfile}"
