make clean
make 
 valgrind --leak-check=full  --show-reachable=no ./qndmpd
