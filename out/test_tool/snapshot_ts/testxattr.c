#include <sys/types.h>
#include <attr/xattr.h>
#include <string.h>
#include <stdio.h>


#include <errno.h>


void xattr_encode(const char *value, size_t size)
{
	static char *encoded;
	static size_t encoded_size;
	int n;
	printf("data\n");

	static const char *digits = "0123456789abcdef";


		for (n = 0; n < size; n++, value++) {
			printf("%c",digits[((unsigned char)*value >> 4)]);
			printf("%c",digits[((unsigned char)*value & 0x0F)]);
		}

	printf("end\n");

}

int do_getxattr(const char *path, const char *name, void *value, size_t size)
{
      return getxattr(path, name, value, size);
}


int main(void){
	int fd;
	int i;
	if ((fd = open("./aclfile", "r")) == -1) {
		printf("open file fail\n");
			return (-1);
	}
	ssize_t size;
	char value[1024];
	size_t value_size;

	char *oriFile="./aclfile";
	char *tarFile="./aclfile_copy";
	char *aclname="security.NTACL";
	int rval;
	printf("start\n");
	value_size = do_getxattr(oriFile, aclname, NULL, 0);
	printf("start value_size=%ld\n",value_size);
	if(value_size){

		rval = do_getxattr(oriFile, aclname, value, value_size);
					if (rval < 0) {
						  fprintf(stderr, "./aclfile: ");
						  fprintf(stderr, "NTACL:%s\n",strerror(errno));
						  return 1;
					}
		printf("rval=%d\n",rval);
		xattr_encode(value,rval);

		// save acl to anther file.
		setxattr(tarFile, aclname, value, rval, 0);
	}


return 0;
}
