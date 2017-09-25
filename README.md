[![Average time to resolve an issue](http://isitmaintained.com/badge/resolution/araujobsd/ndmpd.svg)](http://isitmaintained.com/project/araujobsd/ndmpd "Average time to resolve an issue")
[![Percentage of issues still open](http://isitmaintained.com/badge/open/araujobsd/ndmpd.svg)](http://isitmaintained.com/project/araujobsd/ndmpd "Percentage of issues still open")

# Devel Project #

* Originally this project used to live at: https://bitbucket.org/araujobsd/ndmp-freebsd/
* Now we migrate it to: https://github.com/araujobsd/ndmpd

* Current version is: **1.1**

# NDMPD for FreeBSD #

This is an partial import of ndmpd from Illumos project.
Not all features that ndmp has is working on this version nor the code follow the style(9).

### What I can do with it? ###

* It can accept authentication using PLAIN TEXT and MD5 from ndmp clients.
* It is possible to only move general data between ndmp servers.

### How do I get set up? ###

* checkout the repository.
* type make 
* type make install
* Setup /usr/local/etc/ndmpd.conf
* Run /usr/local/sbin/ndmpd -d

### How did you test it? ###

* I have used ndmjob-spinnaker and also ndmpdcopy(Java).
* I created two FreeBSD vm running ndmpd.
* An third machine to issue the copy between the servers.

### Give me some examples about your test! ###

* **ndmjob**:
vm-cl # ./ndmjob -v -d 7 -o test-data -b 128 -Bdump -D172.17.32.92,marcelo,marcelo -f /tape/200000108600066c.8

**RESULT:**
TEST 0:00:00.003 FINAL test-data Passed -- pass=24 warn=0 fail=0 (total 24)

* **ndmpcopy**:
vm-cl # java -jar ndmpcopy.jar -srchost <HOST1> -srcuser marcelo -srcpass marcelo -srcpath /backup -srcbutype dump -srcenv TYPE=dump -srcenv RECURSIVE=Y -srcenv LEVEL=0 -srcenv HIST=Y -dsthost <HOST2> -dstuser marcelo -dstpass marcelo -dstpath /restore -dstbutype dump -dstenv TYPE=dump -dstenv RECURSIVE=Y -dstenv LEVEL=0

**RESULT:** From HOST1 I copy successfully the content of /backup to HOST2 into /restore 

### What else is necessary to do? ###

* First organize the code, make it build without warnings. 
* Remove a lot of dead functions and duplicate code.
* Put it as close as possible of style(9).
* Rewrite more than half of the code.
* Import from Illumos features such like TAPE, ZFS and TAR.
* Create a new or Import from Illumos **ndmpadm**(*management*)
* Make the proper documentation as well as man pages.

### Contributors  ###
* Marcelo Araujo <araujo@FreeBSD.org> (Developer)
* Nikolai Lifanov <lifanov __at__ mail.lifanov.com> (Tests)

### Vendors tested and OK ###
- FreeBSD 10.1, FreeBSD 9.3 (By Nikolai).
- FreeBSD HEAD (By Araujo).
- **IN PROGRESS** - Oracle ZFS Storage ZS3-2 Appliance with incremental backups LEVEL 0 and 1 (By Nikolai).
