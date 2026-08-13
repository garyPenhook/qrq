#!/usr/bin/env bash
set -Eeuo pipefail

program=$(realpath "${1:?usage: test_ui_sustained_goal.sh program}")
workspace=$(mktemp -d)
trap 'find "$workspace" -depth -delete' EXIT

cp qrqrc toplist callbase.qcb "$workspace"/
sed -i \
  -e 's/^callsign=.*/callsign=TEST/' \
  -e 's/^sessionlength=.*/sessionlength=1/' \
  -e 's/^initialspeed=.*/initialspeed=100/' \
  -e 's/^customitems=.*/customitems=E/' \
  -e 's/^goalspeed=.*/goalspeed=10/' \
  -e 's/^goalduration=.*/goalduration=3/' \
  "$workspace/qrqrc"

expect <<EOF
set timeout 15
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
  "1/2147483647" {}
  timeout { exit 1 }
  eof { exit 1 }
}
after 100
send "E\r"
expect {
  "2/2147483647" {}
  timeout { exit 1 }
  eof { exit 1 }
}
after 100
send "E\r"
after 3500
send "E\r"
expect {
  "Attempt finished" {}
  "Goal met" {}
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
