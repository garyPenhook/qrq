#!/usr/bin/env bash
set -Eeuo pipefail

program=$(realpath "${1:?usage: test_ui_custom_drill.sh program}")
workspace=$(mktemp -d)
trap 'find "$workspace" -depth -delete' EXIT

cp qrqrc toplist callbase.qcb "$workspace"/
sed -i \
  -e 's/^callsign=.*/callsign=TEST/' \
  -e 's/^sessionlength=.*/sessionlength=2/' \
  -e 's/^customitems=.*/customitems=E,T/' \
  "$workspace/qrqrc"

expect <<EOF
set timeout 10
spawn -noecho sh -c "cd '$workspace' && '$program'"
expect {
  "Press any key to continue" {}
  timeout { exit 1 }
  eof { exit 1 }
}
after 100
send "\r"
expect {
  "Please enter your callsign" {}
  timeout { exit 1 }
  eof { exit 1 }
}
after 100
send "\r"
expect {
  "custom: 2 items" {}
  timeout { exit 1 }
  eof { exit 1 }
}
expect {
  "1/2" {}
  timeout { exit 1 }
  eof { exit 1 }
}
after 100
send "X\r"
expect {
  "2/2" {}
  timeout { exit 1 }
  eof { exit 1 }
}
after 100
send "X\r"
expect {
  "Attempt finished" {}
  timeout { exit 1 }
  eof { exit 1 }
}
after 100
send "\r"
expect {
  "Please enter your callsign" {}
  timeout { exit 1 }
  eof { exit 1 }
}
after 100
send "\033\[21~"
expect {
  eof {}
  timeout { exit 1 }
}
EOF
