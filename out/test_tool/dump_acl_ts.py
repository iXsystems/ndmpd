import os

append=".1"

cmd1="find ./ | sort > acl.tmp"
cmd2="cat acl.tmp  | xargs getfacl -a > acl.dump%s"%append
#cmd3="cat acl.tmp  |  ls -lh acl.dump.src | cut -d' ' -f1,4,6,9,13 > acl.dump.mod.%s"%append

print 'cmd1',cmd1
if os.system("%s"%cmd1):
	raise

print 'cmd2',cmd2
if os.system("%s"%cmd2):
	raise
	
#print 'cmd3',cmd3
#if os.system("%s"%cmd3):
#	raise
	
