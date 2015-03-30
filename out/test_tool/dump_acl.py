import os

append=".1"

cmd1="find ./ | sort > acl.tmp"
cmd2="cat acl.tmp  | xargs getfacl socket_output.dump > acl.dump%s"%append

print 'cmd1',cmd1
if os.system("%s"%cmd1):
	raise

print 'cmd2',cmd2
if os.system("%s"%cmd2):
	raise
	
	
