
import random
import os
import sys

KB=1
MB=1024*KB
GB=1024*MB

level=5
folderCount = 10
fileCount = 10
fileSize = 1 # in KB

folderName='dir'
fileName='file'

printout = True

'''
	default content type
	
	0: 1 2 3 4 5 6 7 8 9 0 1 2 3 ...
	1: generate every 1024 randomly, 4 digits
	 
'''



'''
	return the file name.
'''
def createFolder(foldername=''):
	if printout:
		print  foldername
	if not os.path.exists(foldername):
		os.makedirs(foldername)
	
'''
	return the full path of the folder path
'''
def createFile(filename='', size=fileSize):
    
    if printout:
        print 'createFile',filename
    
	cmd = "dd if=/dev/random of=%s bs=1024 count=%d 2> /dev/null"%(filename,size) 
	
	if os.system(cmd)!=0:
		raise
	

def recursiveCreate(path, depth):
	
	createFolder(path)
	
	if depth==level+1:
		return ;
	
	for i in range(1, fileCount+1):
		createFile(path+"/%s%d"%(fileName,i))
	
	for i in range(1, folderCount+1):
		longpath=path+"/%s%d_%d"%(folderName,i,depth)
		recursiveCreate(longpath,depth+1)
		
	
def createTestFiles(path):		
	
	
	global level
	global folderCount 
	global fileCount  
	global fileSize 
	global folderName
	global fileName	
	global printout
	
	
	level=2
	folderCount = 2
	fileCount = 2
	fileSize = 1
	
	folderName='dir'
	fileName='file'
	
	printout = False
	
	longpath=path
	
	print 'create on path',longpath
	
	recursiveCreate(longpath, 1)



if __name__ == '__main__':
	
	longpath=os.getcwd()
	print longpath

	recursiveCreate(longpath, 1)

