import os
import time
import commands

cmd = "ps aux | grep qndmp | grep -v grep | grep -v ps"

while True:
	os.system(cmd)
	time.sleep(1)
