#!/usr/bin/awk -f

/expect: / {
    print substr($0, index($0, "expect: ") + length("expect: "))
}
