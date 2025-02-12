#!/usr/bin/env bash

tmpdir="${TEST_TMPDIR:-/tmp}"
pidfile="${tmpdir}/s_server.pid"

[[ -f "${pidfile}" ]] && kill $(cat "${pidfile}") || true

rm "${pidfile}" || true
rm "${tmpdir}/key.pem" || true
rm "${tmpdir}/cert.pem" || true
