#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

vars = parse_qs(os.getenv("QUERY_STRING"))
try:
    sys.stdout.write("Hello %s %s!\n"%(vars['first_name'][0], vars['last_name'][0]))
except KeyError:
    sys.stdout.write("Hello world!\n")
