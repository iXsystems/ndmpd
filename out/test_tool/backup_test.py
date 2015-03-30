'''
	backup_test.py
		backup folder "srcpath" from  host "srchost" to local path "dstpath"
'''
import os

srchost="172.17.33.43"
srcuser="ndmpaccess"
srcpass="ndmpaccess"
srcpath="/share/ndmpshare"
dstpath="/tmp/ndmp/"
SNAPSURE="n"
LEVEL="0"
QNAP_FILE01="exfile*"

cmd='java -jar ndmpcopy.jar '
info='-srchost %s -srcuser %s -srcpass %s -srcbutype dump -srcpath %s -dstpath %s '%(srchost,srcuser,srcpass,srcpath,dstpath)
env='-srcenv SNAPSURE=%s -srcenv LEVEL=%s -srcenv QNAP_FILE01=%s'%(SNAPSURE, LEVEL, QNAP_FILE01)

cmd=cmd+info+env
print 'cmd=',cmd
os.system(cmd);

