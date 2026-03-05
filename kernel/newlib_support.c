#include <errno.h>
#include <sys/types.h>
/* メモリの動的管理が実現されていない状態でsprintfを動かすためのスタブ */
caddr_t sbrk(int incr){
    errno = ENOMEM;
    return (caddr_t)-1;
}