
import os

rsPath="/share/ndmpd_spa"
rtPath="/share/ndmpd_spb"

lsPath="/share/ndmpd_spa"
ltPath="/share/ndmpd_spb"


def mountFS(srcIP,tgtIP):
	cmd="umount -f %s"%(lsPath)
	print 'exec:',cmd
	cmd="mount %s:%s %s"%(srcIP, rsPath,lsPath)
	print 'exec:',cmd
	if os.system(cmd)==0:
		print cmd,"success~~~~~~~~"

	cmd="umount -f %s"%(ltPath)
	print 'exec:',cmd
		

	cmd="mount %s:%s %s"%(tgtIP, rtPath,ltPath)
	print 'exec:',cmd
	if os.system(cmd)==0:
		print cmd,"success~~~~~~~~~"

mountFS("172.17.22.67","172.17.22.68")
