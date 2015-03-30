
import os

def remote_copy(src_host,src_usename,src_password,src_path,tgt_host,tgt_usename,tgt_password,tgt_path):
    print "aaa"
    """
    java -jar ndmpcopy.jar 
    -srchost 172.17.22.67 -srcuser bart -srcpass ABcd_1234 -srcpath /share/ndmpd_spa/ -srcbutype dump -srcenv TYPE=dump -srcenv RECURSIVE=Y -dstenv FILESYSTEM=/share/ndmpd_spa/ -srcenv HIST=Y 
    -dsthost 172.17.22.68 -dstuser bart -dstpass ABcd_1234 -dstpath /share/ndmpd_spb/ -dstbutype dump -dstenv TYPE=dump -dstenv RECURSIVE=Y 
    """
    src_tgt="java -jar ndmpcopy.jar"
    src_tgt = src_tgt+ " -srchost %s -srcuser %s -srcpass %s -srcpath %s -srcbutype dump -srcenv TYPE=dump -srcenv RECURSIVE=Y -srcenv HIST=Y "%(src_host,src_usename,src_password,src_path)
    src_tgt = src_tgt+ " -dsthost %s -dstuser %s -dstpass %s -dstpath %s -dstbutype dump -dstenv TYPE=dump -dstenv RECURSIVE=Y -dstenv FILESYSTEM=%s "%(tgt_host,tgt_usename,tgt_password,tgt_path,src_path)

    if os.system("%s"%src_tgt):
        raise


src_host="172.17.22.67"
src_usename="bart"
src_password="ABcd_1234"
src_path="/share/ndmpd_spa"


tgt_host="172.17.22.68"
tgt_usename="bart"
tgt_password="ABcd_1234"
tgt_path="/share/ndmpd_spb"

print "ccc"

remote_copy(src_host,src_usename,src_password,src_path,tgt_host,tgt_usename,tgt_password,tgt_path)
