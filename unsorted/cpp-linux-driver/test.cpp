#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>

int main() {
    printf("fopen...\n");
    FILE* f = fopen("/dev/uwcajoychar", "rb");
    if (!f) {
        printf("fopen failed, errno = %s (%d)\n", strerror(errno), errno);
        return 1;
    }
    printf("fopen ok\n");
    
    for (int i=0; i<5; ++i) {
        uint8_t x = 0;
        auto read = fread(&x, 1, 1, f);
        if (read == 1) {
            printf("read: %d\n", x);
        }
        else {
            printf("read error, errno = %s (%d)\n", strerror(errno), errno);
        }
    }
    
    printf("fclose...\n");
    fclose(f);
    printf("fclose ok\n");
}
