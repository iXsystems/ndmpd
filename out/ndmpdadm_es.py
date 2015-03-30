#!/usr/local/bin/python
# -*- coding: utf-8 -*-




"""
	Create By Bart.Hsiao@QNAP

	ndmpadm is a CLI for controller ndmpd.

	1. Setup username and password for plain text or MD5
	2. Generate base64 encoding. 
	3. Setup listen IP
	4. Setup serve IP
	5. Setup to use restore full-path or not.
	6. show configuration 
	7. get network interface list.
	8. restart daemon
	9. show last backup information.
"""

import sys

"""
	input key:value pair
	return  0: success
		   -1: write fail
		   -2: input error
"""
def writeConf(keyval, path='/etc/ndmpd.conf'):
	tmpcontent=""
	try:
		f = open(path, "r")
		while True:
			line = f.readline()
			if not line:
				break
			line=line.strip()
			try:
				tmp=line.split("=",1);
				if tmp[0].strip() in keyval.keys():
					tmpcontent="%s%s=%s\n"%(tmpcontent, tmp[0].strip(),keyval[tmp[0].strip()])
				else:
					tmpcontent="%s%s\n"%(tmpcontent,line)
			except:
				pass
	except:
		return -1

	f.close()

	try:
		f = open(path, "w")
		f.write(tmpcontent);
		f.close()
	except:
		return -1
	finally:
		f.close()


	return 0




"""
	return key:value map.
"""
def readConf(path='/etc/ndmpd.conf'):

	f = open(path, "r")
	keyval={}
	try:
		keyval={}
		while True:
			line = f.readline()
			if not line:
				break
			line=line.strip()
			if line.startswith('#'):
				continue
			try:
				tmp=line.split("=",1);
				keyval[tmp[0].strip()]=tmp[1].strip()
			except:
				pass

	except:
		return keyval
	f.close()
	return keyval



def help():
	print ""
	print "usage:"
	print "ndmpdadm [username $username] [password password] [username_md5 $username] [password_md5 password] [lnic $interface] [snic $interface] [rsfullpath $bool]|[showconfig]|[showlastbackup]"
	print ""
	print "username=$username\tsetup clear text user name"
	print "username_md5=$username\tsetup md5 user name"
	print "password=$password\tsetup clear text password"
	print "password_md5=$password\tsetup md5 password"
	print "lnic=$interface\t\tsetup network interface for listening"
	print "snic=$interface\t\tsetup network interface for data transfer"
	print "rsfullpath=$bool\tsetup to use full-path restore or not, $bool is enable/disable"
	print "showconfig\t\tshow current ndmpd configuration"
	print "listnic\t\t\tshow all intefaces"
	print "showlastbackup\t\tshow last backup date"
	print ""

import socket
def isValidNIC(nic):
	try:
		nics=getNetNICs()
		if nic in nics:
			return True
		else:
			return False
	except:
		return False

def isBool(val):
	if val=="TRUE" or val=="FALSE":
		return True
	return False


def showConfig():
	keyval = readConf()
	for key in sorted(keyval):
		print "%s:%s"%(key,keyval[key])
import base64
def toBase64(val):
	'''
		our UI will do this. We just skip this here.
	'''
	ret=""
	ret=base64.b64encode(val);
	if base64.b64decode(ret)!=val:
		return ""
	else:
		return ret
	


import subprocess
def getNetNICs():

	proc = subprocess.Popen(["/sbin/ifconfig", ""], stdout=subprocess.PIPE, shell=True)
	(output, err) = proc.communicate()

	nics=[]
	nic=""
	nicipmap={}
	for line in output.split("\n"):
		if len(line.strip())==0:
			continue
				
		if ord(line[0])>=48 and ord(line[0])<=122:			
			if line.startswith('ntb') or line.startswith('tun') or \
				line.startswith('ipfw') or line.startswith('lo'):
				nic=''
				continue
						
			if line.find("flags")!=-1:
				nic = line.split(':')[0]
			else:
				nic=line.split("Link")[0];
			nicipmap[nic]=''
		else:
			line=line.strip();
			if line.startswith('inet ') and nic!='':
				
				if nicipmap[nic]=='':
					nicipmap[nic]=line.split(' ')[1]
				else:
					nicipmap[nic]="%s,%s"%(nicipmap[nic],line.split(' ')[1])

	return nicipmap


def showNetNICs():
	nicipmap=getNetNICs()
	for nic in sorted(nicipmap):
		print "%s(%s)"%(nic,nicipmap[nic])

def showLastbackup(path='/var/log/ndmp/dumpdates'):
	f = open(path, "r")
	title=0
	if f:
		while True:
			line = f.readline()
			if not line:
				break
			line=line.strip()
			try:
				tmp=line.split("\t");
				if title==0:
					print 'Backup Path\t\t\tBackup Level\tBackup Date'
					title=1
				print "%s\t\t\t%s\t%s"%(tmp[0],tmp[1],tmp[2])
			except:
				pass
	
		f.close()
	else:
		print ''

import os
def start(path="/nas/sbin"):
	os.system(path+"./qndmpd -f /etc/ndmpd.conf &")
def stop():
	os.system("pkill qndmpd")
def restart(path="/nas/sbin"):
	os.system("pkill qndmpd;"+ path+"./qndmpd -f /etc/ndmpd.conf &")

if __name__ == "__main__":

	args=sys.argv[1:]
	retmsg={}
	form={}
	
	if len(args)==0:
		help();
		sys.exit(1)
	elif len(args)==1:
		if args[0]=="listnic":
			showNetNICs()
		elif args[0]=="showconfig":
			showConfig()
		elif args[0]=="showlastbackup":
			showLastbackup()
		elif args[0]=="start":
			start("")
		elif args[0]=="stop":
			stop()
		elif args[0]=="restart":
			restart("")	
		else:
			help()
			sys.exit(1)
	else:

		try:
			for arg in args:
				tmp=arg.split("=")
				form[tmp[0]]=tmp[1]
		except:
			help()
			sys.exit(1)

		username=""
		password=""
		username_md5=""
		password_md5=""
		lnic=""
		snic=""
		rsfullpath=""

		if 'username' in form:
			username=form['username']
		if 'password' in form:
			password=form['password']
		if 'username_md5' in form:
			username_md5=form['username_md5']
		if 'password_md5' in form:
			password_md5=form['password_md5']
		if 'lnic' in form:
			lnic=form['lnic']
		if 'snic' in form:
			snic=form['snic']
		if 'rsfullpath' in form:
				rsfullpath=form['rsfullpath']
		keyval={}
		# user must give username and password
		keyval["cleartext-username"]=username
		keyval["cram-md5-username"]=username_md5
		keyval["cleartext-password"]=toBase64(password)
		keyval["cram-md5-password"]=toBase64(password_md5)

		if lnic!="":
			keyval["listen-nic"]=lnic
		if snic!="":
			keyval["serve-nic"]=snic
		else:
			keyval["serve-nic"]=lnic
		if rsfullpath!="":
			keyval["restore-fullpath"]=rsfullpath
		#print keyval, len(keyval)
		
		if writeConf(keyval)!=0:
			message='Write config file fail'
		else:
			message='Write config success'

		print message

