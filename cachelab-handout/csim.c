#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define UN_INIT     (0xf0f)

int hits = 0;       /* number of  hits */
int misses = 0;     /* number of misses */
int evictions = 0;  /* number of evictions */

uint8_t verbose_flag = 0;
uint8_t help_info_flag = 0;
uint8_t set_index_bit;
uint8_t block_offset_bit;
uint32_t line_num;
char* trace_file = NULL;

struct link_list_node {
    uint64_t tag;
    struct link_list_node* next;
};

struct LRU_cache {
    uint64_t size;
    struct link_list_node* head;
    struct link_list_node* tail;
};

struct LRU_cache* m_cache;

void set_opt(int argc, char* argv[]) {
    char opt;
    uint8_t required_cnt = 0;
    const char* opt_string = "hvs:E:b:t:";
    while ((opt = getopt(argc, argv, opt_string))!= -1) {
        switch (opt) {
            case '?':
            case 'h':
                help_info_flag = 1;
                return;
            case 'v':
                verbose_flag = 1;
                break;
            case 's':
                ++required_cnt;
                set_index_bit = atoi(optarg);
                break;
            case 'E':
                ++required_cnt;
                line_num = atoi(optarg);
                break;
            case 'b':
                ++required_cnt;
                block_offset_bit = atoi(optarg);
                break;
            case 't':
                trace_file = (char*)malloc(strlen(optarg) + 1);
                if (trace_file == NULL) {
                    printf("malloc error for file name!\n");
                    exit(0);
                }
                strcpy(trace_file, optarg);
                ++required_cnt;
        }
    }
    if (required_cnt != 4) {
        printf("%s: Missing required command line argument\n", argv[0]);
        help_info_flag = 1;
    }
}

void print_help_info(char* argv[]) {
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n"
           "Options:\n"
           "  -h         Print this help message.\n"
           "  -v         Optional verbose flag.\n"
           "  -s <num>   Number of set index bits.\n"
           "  -E <num>   Number of lines per set.\n"
           "  -b <num>   Number of block offset bits.\n"
           "  -t <file>  Trace file.\n"
           "\n"
           "Examples:\n"
           "  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n"
           "  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0], argv[0], argv[0]);
}

uint64_t get_set_id(uint64_t address) {
    static uint64_t mask = UN_INIT;
    if (mask == UN_INIT) {
        mask = 0;
        for (int i = block_offset_bit;
            i < block_offset_bit + set_index_bit; ++i) {
            mask |= (1ull<<i);
        }
    }
    return (address & mask)>>(block_offset_bit);
}

uint64_t get_tag(uint64_t address) {
    static uint64_t mask = UN_INIT;
    if (mask == UN_INIT) {
        mask = 0;
        for (int i = block_offset_bit + set_index_bit; i < 64; ++i) {
            mask |= (1ull<<i);
        }
    }
    return (address & mask)>>(block_offset_bit + set_index_bit);
}

struct link_list_node* search_line(struct LRU_cache* cache, uint64_t tag) {
    struct link_list_node* cur = cache->head;
    while (cur != NULL) {
        if (cur->tag == tag) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void move_to_tail(struct LRU_cache* cache, struct link_list_node* target) {
    if (cache->tail == target) return;
    assert(cache->tail->next == NULL);
    struct link_list_node* cur = cache->head;
    struct link_list_node* pre = NULL;
    while (cur != NULL) {
        if (cur == target) {
            if (pre != NULL) {
                pre->next = cur->next;
            } else {
                cache->head = cur->next;
            }
            cache->tail->next = cur;
            cache->tail = cur;
            cur->next = NULL;
            break;
        }
        pre = cur;
        cur = cur->next;
    }
    assert(cache->tail->next == NULL);
}

void data_access(uint64_t address, uint32_t size) {
    uint64_t set_id = get_set_id(address);
    uint64_t tag = get_tag(address);
    struct link_list_node* target = search_line(&m_cache[set_id], tag);

    if (target == NULL) {
        ++misses;
        if (verbose_flag) {
            printf(" miss");
        }

        if (m_cache[set_id].size < line_num) {
            m_cache[set_id].size++;
            struct link_list_node *new_node =
                    (struct link_list_node *) malloc(sizeof(struct link_list_node));
            new_node->next = NULL;
            new_node->tag = tag;
            if (m_cache[set_id].size == 1) {
                m_cache[set_id].head = m_cache[set_id].tail = new_node;
            } else {
                m_cache[set_id].tail->next = new_node;
                m_cache[set_id].tail = new_node;
            }
        } else { //evict
            ++evictions;
            if (verbose_flag) {
                printf(" eviction");
            }

            m_cache[set_id].head->tag = tag;
            move_to_tail(&m_cache[set_id], m_cache[set_id].head);
        }
    } else {
        ++hits;
        if (verbose_flag) {
            printf(" hit");
        }

        move_to_tail(&m_cache[set_id], target);
    }

}

int main(int argc, char* argv[]) {
    set_opt(argc, argv);
    if (help_info_flag) {
        print_help_info(argv);
        return 0;
    }
    m_cache = (struct LRU_cache*)malloc(sizeof(struct LRU_cache) * (1ull<<set_index_bit));
    memset(m_cache, 0x00, sizeof(struct LRU_cache) * (1ull<<set_index_bit));
    FILE* fp = fopen(trace_file, "r");
    char action;
    uint64_t address;
    uint32_t size;
    while (fscanf(fp, " %c %lx,%d", &action, &address, &size) != EOF) {
        if (action == 'I') continue;
        if (verbose_flag) {
            printf("%c %lx,%d", action, address, size);
        }
        data_access(address, size);
        if (action == 'M') {
            data_access(address, size);
        }
        if (verbose_flag) {
            printf("\n");
        }
    }
    fclose(fp);
    printSummary(hits, misses, evictions);
    return 0;
}
