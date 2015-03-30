
import random
import os
import sys

KB=1
MB=1024*KB
GB=1024*MB
fileSize = 10*GB # in KB

filename="10GFile"
print 'createFile',filename
    
cmd = "dd if=/dev/random of=%s bs=1024 count=%d 2> /dev/null"%(filename,fileSize) 

if os.system(cmd)!=0:
	raise
	


	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
