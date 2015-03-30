1. use back.py to back data to local
2. use loop.py to do three-way backup.
3. use command "rsync -rvcn  $sourcehost:$path $localpath" to compare.
4. output acl to files using dump_acl.py
5. compare two acl.dump