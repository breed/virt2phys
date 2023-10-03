#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>

/* there are 4 levels of 9 bits that is 36 bits or 9 hex digits followed
 * by 12 bits of offset into the page or 3 hex digits. we only want the
 * page address bits */
const uint64_t PTE_addr_mask = 0xfffffffff000LL;

static void split_addr(uint64_t addr)
{
    printf("addr %016lX -> ", addr);
    for (
            int i = 3; i >= 0; i--
            ) {
        printf("%03lX ", (addr >> (12 + 9 * i)) & 0x1ff);
    }
    printf("%03lX\n", addr & 0xfff);
}

static void dump_table(uint64_t addr, uint64_t *table)
{
    printf("PAGE TABLE for %016lX (present entries):\n", addr);
    for (
            int i = 0; i < 512; i++
            ) {
        if (table[i] & 1) {
            printf("  %03X %016lx\n", i, table[i]);
        }
    }
}

static uint64_t resolve_entry(int fd, uint64_t table, uint64_t addr, int depth)
{
    if (depth == 0) {
        return table;
    }
    /* shift out the offset bits (12) and then the 9 bits for each depth:
     * the p4 table is the 4th set of 9 bits. subtract 1 since no shift needed
     * for bottom page table index... */
    int entry_index = (addr >> (12 + 9 * (depth - 1))) & 0x1ff;
    table &= PTE_addr_mask;
    printf("Need to resolved entry %03X in %016lX\n", entry_index, table);
    uint64_t page[512];
    lseek(fd, table, SEEK_SET);
    int rc = read(fd, page, sizeof(page));
    dump_table(table, page);
    if (rc != sizeof(page)) {
        printf("Got %d reading %016lX\n", rc, table);
        return 0;
    }
    printf("Got PTE %016lX\n", page[entry_index]);
    return resolve_entry(fd, page[entry_index] & ~0xfff, addr, depth - 1);
}

static uint64_t resolve_phys_addr(int fd, uint64_t p4, uint64_t addr, const
char *name)
{
    printf("%s(): ", name);
    split_addr(addr);
    uint64_t phys = resolve_entry(fd, p4, addr, 4);
    printf("%s(): virt %016lX -> phys %016lX\n", name, addr, phys+(addr&0xfff));
    printf("------------------\n");
}

int main()
{
    /* both of these /proc files come from the cr3 kernel module, which
     * you will need to download, build, and insmod */
    int fd = open("/proc/page_reader", O_RDONLY);
    if (fd == -1) {
        perror("/proc/page_reader");
        exit(30);
    }

    FILE *fh = fopen("/proc/cr3", "r");
    if (fh == NULL) {
        perror("/proc/cr3");
        exit(2);
    }

    char buff[4096];
    int md = open("/proc/self/maps", O_RDONLY);
    int rc = read(md, buff, sizeof buff);
    write(1, buff, rc);
    /* CR3 has the address of the top level page table, but we need
     * privileges to read it hence the kernel module */
    uint64_t p4;
    if (fscanf(fh, "CR3=%lX\n", &p4) <= 0) {
        fprintf(stderr, "Couldn't read CR3\n");
        exit(3);
    }
    printf("CR3 is %lX\n", p4);

    resolve_phys_addr(fd, p4, (uint64_t) buff, "buff");
    exit(0);
}
