#include <sys/types.h>
#include <attr/xattr.h>
#include <string.h>
#include <stdio.h>

#include <sys/acl.h>
#include <errno.h>


int main(void){
	ssize_t size;
	acl_t acl = NULL;
	char *acltp;

	 acl = acl_get_file("./aclfile", ACL_TYPE_ACCESS);

	int acllen= -1;
	if (acl && (acltp = acl_to_text(acl, 0)) != NULL) {
		acllen= strlen(acltp);
	}
	printf("len=%d \nACL\n!%s!",acllen,acltp);

	acl = acl_from_text(acltp);
	if (acl) {
		printf("load success\n");
	}else
		printf("load fail!!!!!\n");

return 0;
}
