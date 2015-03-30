import os 
src="172.17.32.89"
spath="/share/dv2_1"
localpath="c:/tmp/es.dump"
snapshot='Y'
str="java -jar ndmpcopy.jar -srchost %s -srcuser bart -srcpass ABcd_1234 -srcbutype dump -srcpath %s -dstpath %s -srcenv SNAPSURE %s -dstenv SNAPSURE %s"%(src,spath,localpath,snapshot)

print 'cmd',str
for i in range(0,1):
	os.system(str);
