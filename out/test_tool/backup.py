import os 
src="172.17.32.44"
spath="/share/dv1_2/"
localpath="c:/tmp/es.dump"
snapshot='y'
name="ndmpaccess"
password="ndmpaccess"
level='0'
str="java -jar ndmpcopy.jar -srchost %s -srcuser %s -srcpass %s -srcbutype dump -srcpath %s -dstpath %s -srcenv SNAPSURE=%s -srcenv LEVEL=%s"%(src,name,password,spath,localpath,snapshot,level)

print 'cmd',str
#for i in range(0,1000000):
while True:
	os.system(str);
