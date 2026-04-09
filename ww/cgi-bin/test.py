#!/usr/bin/env python3
import os, sys

method = os.environ.get('REQUEST_METHOD', '')
qs     = os.environ.get('QUERY_STRING', '')
cl     = int(os.environ.get('CONTENT_LENGTH', 0))
body   = sys.stdin.read(cl) if cl > 0 else ''

print('Content-Type: text/plain')
print()
print('METHOD=' + method)
print('QS='     + qs)
print('BODY='   + body)
