#!/bin/sh

MESON_LOG_FILE="./meson-logs/meson-log.txt"
GREP_PATTERN="Failed to guess install tag"

if ! [ -f "${MESON_LOG_FILE}" ]; then
  echo "Error: Could not find '${MESON_LOG_FILE}' file." >&2
  exit 1
fi

UNTAGGED_FILES=$(grep -c "${GREP_PATTERN}" <${MESON_LOG_FILE})
if [ "${UNTAGGED_FILES}" -ne "0" ]; then
  echo "There are ${UNTAGGED_FILES} files without a install tag." >&2
  grep "${GREP_PATTERN}" <${MESON_LOG_FILE}
  exit 1
fi
