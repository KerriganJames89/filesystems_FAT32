#define ATTR_READ_ONLY 0b1
#define ATTR_HIDDEN 0b10
#define ATTR_SYSTEM 0b100
#define ATTR_VOLUME_ID 0b1000
#define ATTR_DIRECTORY 0b10000
#define ATTR_ARCHIVE 0b100000
#define ATTR_LONG_NAME 0b1111
#define ATTR_SHORT_NAME 0b0000

#define TRUE 1
#define FALSE 0

/******************* Structs *******************/

typedef struct
{
    int size;
    char **items;
} tokenlist;

typedef struct
{
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t number_of_fats;
    uint16_t root_entry_count;
    uint8_t _unused_0[13];
    uint32_t total_sectors;
    uint32_t sectors_per_fat;
    uint8_t _unused_1[4];
    uint32_t root_dir_first_cluster;
} __attribute__((packed)) BootSector;

typedef struct
{
    unsigned char short_file_name[8];
    unsigned char short_file_extension[3];
    uint8_t attributes;
    uint8_t _unused_0;
    uint8_t creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high_word;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low_word;
    uint32_t file_size;
} __attribute__((packed)) DirectoryEntry;

typedef struct Open_File_Table
{
    char file_name[256];
    long offset;
    char mode[2];
    TAILQ_ENTRY (Open_File_Table) files;
} Open_File_Table;

TAILQ_HEAD(, Open_File_Table) head;

struct timeval tv;

/****************** Global Variables *****************/

long current_cluster_number = 0;

long current_directory = 0;

int quit = FALSE;

long previous_cluster_low = 0;

long total_size_of_files = 0;

long total_size_remaining = 0;

/******************* Prototypes **********************/

// ********************************
// Parse user input
// ********************************

char *get_input (void);

tokenlist *get_tokens (char *input);

tokenlist *new_tokenlist (void);

void add_token (tokenlist *tokens, char *item);

void free_tokens (tokenlist *tokens);

// ********************************

void execute (const tokenlist *tokens, FILE *fp, BootSector *bs);

BootSector boot_parse (FILE *fp);

void print_boot_sector (const BootSector *bs);

DirectoryEntry parse_directory_entry (FILE *fp, long offset);

void print_directory_entry (const DirectoryEntry *dir);

int get_fat_size_in_sectors (BootSector const *bs);

int get_first_data_sector (BootSector const *bs);

long get_cluster_offset (BootSector const *bs, int cluster);

long get_fat_offset (BootSector const *bs);

uint32_t get_next_cluster (FILE *fp, BootSector const *bs, int current_cluster);

DirectoryEntry *get_file_size (BootSector const *bs, FILE *fp, char *file_name, int should_print);

uint32_t get_directory_entry_first_cluster (DirectoryEntry const *dir);

int last_cluster_marker (uint32_t cluster);

void list_directory (BootSector const *bs, FILE *fp);

void change_directory (BootSector const *bs, const tokenlist *tokens, FILE *fp);

void remove_spaces (char *s);

uint32_t read_fat (FILE *fp, BootSector *bs, int current_cluster);

void write_fat (FILE *fp, BootSector *bs, int cluster, uint32_t value);

int is_last_cluster_marker (uint32_t cluster);

int get_next_free_cluster (FILE *fp, BootSector *bs, int search_start);

int get_file_last_cluster (FILE *fp, BootSector *bs, int file_first_cluster);

uint16_t get_current_date ();

uint16_t get_current_time ();

uint8_t get_current_miliseconds ();

int create_empty_file (FILE *fp, BootSector *bs, int directory_first_cluster, char *filename, int is_directory);

void convert_string (char *str);

long files_cluster_number (BootSector const *bs, FILE *fp, long current_offset);

long space_remaining (BootSector const *bs, FILE *fp, long cluster_number_of_files);

DirectoryEntry get_entry (BootSector const *bs, FILE *fp);

void add_open_list (char *file_name, char *mode);

void remove_item_open_list (char *file_name);

int exist_in_file_table (char *file_name, int is_open);

void print_open_file_table ();

int valid_mode (char *mode);

char *read_file (FILE *fp, BootSector *bs, int file_size, int size, int offset);

void write_file (FILE *fp, BootSector *bs, DirectoryEntry *entry, int size, int offset, char *str);

void lseek (char *file_name, long offset);

int get_offset_file_table (char *file_name);

char *get_mode_file_table (char *file_name);

int remove_file (FILE *fp, BootSector *bs,
                 int directory_first_cluster_idx, char *filename);

int get_number_of_clusters(FILE *fp, BootSector *bs, int file_first_cluster);

void add_spaces (char *str, int num);

void remove_quotes (char *str);

/******************* End Of Prototypes ******************/