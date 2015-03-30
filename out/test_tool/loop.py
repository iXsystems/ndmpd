import os



src_host="172.17.32.94"
#src_host="172.17.32.62"
src_usename="ndmpaccess"
src_password="ndmpaccess"
src_path="/share/src/"
#src_path="/vol/test/"


tgt_host="172.17.32.91"
#tgt_host="172.17.32.62"
tgt_usename="ndmpaccess"
tgt_password="ndmpaccess"
#tgt_path="/share/target/"
tgt_path="/share/dv1_2/"
#tgt_path="/vol/test/"


src_tgt="java -jar ndmpcopy.jar"
src_tgt = src_tgt+ " -srchost %s -srcuser %s -srcpass %s -srcpath %s -srcbutype dump -srcenv TYPE=dump -srcenv RECURSIVE=Y -srcenv LEVEL=0 -srcenv HIST=Y -srcenv SNAPSURE=n"%(src_host,src_usename,src_password,src_path)
src_tgt = src_tgt+ " -dsthost %s -dstuser %s -dstpass %s -dstpath %s -dstbutype dump -dstenv TYPE=dump -dstenv RECURSIVE=Y -dstenv LEVEL=0 -dstenv FILESYSTEM=%s "%(tgt_host,tgt_usename,tgt_password,tgt_path,src_path)

print 'cmd',src_tgt

#test with exclude, need to seperate by ',' (only support file)
#exclude_file_list="file1,file2"
#src_tgt = src_tgt+ " -srcenv EXCLUDE=\"%s\" "%(exclude_file_list)
#if os.system("%s"%src_tgt):
#	raise


#backstr="java -jar ndmpcopy.jar -srchost 172.17.28.143 -srcuser bart -srcpass ABcd_1234  -srcbutype dump -srcpath /share/spb_ndmp -dsthost 172.17.28.144 -dstuser bart -dstpass ABcd_1234 -dstpath /share/spb_ndmp -dstenv TYPE=dump -dstenv RECURSIVE=Y -dstenv FILESYSTEM=/share/spb_ndmp/ -srcenv TYPE=dump -srcenv RECURSIVE=Y -srcenv HIST=Y"
i=0
while True:
	i=i+1
	print '---------------------start count ============= ',i
	os.system(src_tgt);
	if i==1:
		break;
	print '---------------------end count ============= ',i
	
	
	
	
	

	
