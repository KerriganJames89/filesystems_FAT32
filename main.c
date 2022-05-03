#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/queue.h>
#include "fat.h"

int main (void)
{
    static const char *const file_name = "fat32.img";

    FILE *fp = fopen (file_name, "rb+");
    if (!fp)
    {
        fprintf (stderr, "Failed to open file '%s'\n", file_name);
        exit (1);
    }

    BootSector bs = boot_parse (fp);
    current_cluster_number = bs.root_dir_first_cluster;
    current_directory = bs.root_dir_first_cluster;
    previous_cluster_low = 0;

    TAILQ_INIT(&head);

    total_size_of_files = files_cluster_number (&bs, fp, current_directory);
    total_size_remaining = space_remaining (&bs, fp, total_size_of_files);
    //printf("total size: %d bytes\ntotal remaining: %d bytes\n", total_size_of_files, total_size_remaining);

    /* User interface to FAT32 */
    while (!quit)
    {
        printf ("$ ");

        /* get user input of tokens */
        char *input = get_input ();
        tokenlist *tokens = get_tokens (input);
        execute (tokens, fp, &bs);

        /* free dynamically allocated memory */
        free (input);
        free_tokens (tokens);
    }
    /* close file and return from program */
    fclose (fp);

    return 0;
}

/******************* Start Of Definitions *******************/

tokenlist *new_tokenlist (void)
{
    tokenlist *tokens = (tokenlist *) malloc (sizeof (tokenlist));
    tokens->size = 0;
    tokens->items = (char **) malloc (sizeof (char *));
    tokens->items[0] = NULL; /* make NULL terminated */
    return tokens;
}

void add_token (tokenlist *tokens, char *item)
{
    int i = tokens->size;
    tokens->items = (char **) realloc (tokens->items, (i + 2) * sizeof (char *));
    tokens->items[i] = (char *) malloc (strlen (item) + 1);
    tokens->items[i + 1] = NULL;
    strcpy(tokens->items[i], item);
    tokens->size += 1;
}

char *get_input (void)
{
    char *buffer = NULL;
    int bufsize = 0;
    char line[5];

    while (fgets (line, 5, stdin) != NULL)
    {
        int addby = 0;
        char *newln = strchr (line, '\n');

        if (newln != NULL)
        {
            addby = newln - line;
        } else
        {
            addby = 4;
        }

        buffer = (char *) realloc (buffer, bufsize + addby);
        memcpy(&buffer[bufsize], line, addby);
        bufsize += addby;

        if (newln != NULL)
        {
            break;
        }
    }
    buffer = (char *) realloc (buffer, bufsize + 1);
    buffer[bufsize] = 0;

    return buffer;
}

tokenlist *get_tokens (char *input)
{
    char *buf = (char *) malloc (strlen (input) + 1);
    strcpy(buf, input);
    tokenlist *tokens = new_tokenlist ();
    char *tok = strtok (buf, " ");

    while (tok != NULL)
    {
        add_token (tokens, tok);
        tok = strtok (NULL, " ");
    }
    free (buf);

    return tokens;
}

void free_tokens (tokenlist *tokens)
{
    int i;
    for (i = 0; i < tokens->size; i++)
    {
        free (tokens->items[i]);
    }
    free (tokens);
}

void execute (const tokenlist *tokens, FILE *fp, BootSector *bs)
{
    if (tokens->items[0] == NULL)
    {
        return;
    }
    /* check for valid command */
    if (strcmp (tokens->items[0], "exit") == 0)
    {
        quit = TRUE;
    } else if (strcmp (tokens->items[0], "info") == 0)
    {
        print_boot_sector (bs);
    } else if (strcmp (tokens->items[0], "size") == 0 && tokens->size == 2)
    {
        get_file_size (bs, fp, tokens->items[1], TRUE);
    } else if (strcmp (tokens->items[0], "ls") == 0)
    {
        if (tokens->size == 2)
        {
            DirectoryEntry *entry = get_file_size (bs, fp, tokens->items[1], FALSE);
            if (entry == NULL)
            {
                return;
            } else if (entry->attributes == ATTR_DIRECTORY)
            {
                change_directory (bs, tokens, fp);
                list_directory (bs, fp);
                /* null out token->items[1] */
                strncpy(tokens->items[1], "\0", strlen (tokens->items[1]));
                /* go back a dir */
                strncpy(tokens->items[1], "..", strlen (".."));
                change_directory (bs, tokens, fp);
            }
        } else
        {
            list_directory (bs, fp);
        }
    } else if (strcmp (tokens->items[0], "cd") == 0)
    {
        change_directory (bs, tokens, fp);
    } else if (strcmp (tokens->items[0], "creat") == 0 && tokens->size == 2)
    {
        /* make string 8 bits */
        convert_string (tokens->items[1]);
        create_empty_file (fp, bs, (int) current_directory, tokens->items[1], FALSE);
    } else if (strcmp (tokens->items[0], "mkdir") == 0 && tokens->size == 2)
    {
        /* make string 8 bits */
        convert_string (tokens->items[1]);
        create_empty_file (fp, bs, (int) current_directory, tokens->items[1], TRUE);
    } else if (strcmp (tokens->items[0], "open") == 0 && tokens->size == 3)
    {
        // add to linked list
        DirectoryEntry *entry = get_file_size (bs, fp, tokens->items[1], FALSE);
        if (entry == NULL)
        {
            return;
        } else if (entry->attributes == ATTR_DIRECTORY)
        {
            printf ("Error: not a file\n");
        } else if (entry != NULL &&
                   valid_mode (tokens->items[2]) &&
                   exist_in_file_table (tokens->items[1], TRUE) == FALSE)
        {
            add_open_list (tokens->items[1], tokens->items[2]);
        }
    } else if (strcmp (tokens->items[0], "close") == 0)
    {
        if (exist_in_file_table (tokens->items[1], FALSE))
        {
            remove_item_open_list (tokens->items[1]);
        } else
        {
            printf ("Error: file not found\n");
        }
    } else if (strcmp (tokens->items[0], "read") == 0 && tokens->size == 3)
    {
        /* get mode and check if file can be read */
        char *mode = get_mode_file_table (tokens->items[1]);
        if (strcmp (mode, "r") == 0 || strcmp (mode, "wr") == 0 || strcmp (mode, "rw") == 0)
        {
            /* get offset from file table */
            int offset = get_offset_file_table (tokens->items[1]);
            int size = atoi (tokens->items[2]);
            DirectoryEntry *entry = get_file_size (bs, fp, tokens->items[1], FALSE);
            char *str = read_file (fp, bs, entry->file_size, size, offset);

            /* print results to console */
            printf ("%s\n", str);
        } else
        {
            printf ("Error: cant read file\n");
        }
    } else if (strcmp (tokens->items[0], "lseek") == 0 && tokens->size == 3)
    {
        int offset = atoi (tokens->items[2]);
        DirectoryEntry *entry = get_file_size (bs, fp, tokens->items[1], FALSE);

        if (entry == NULL)
        {
            return;
        } else if (offset > entry->file_size)
        {
            printf ("Error: running lseek\n");
        } else
        {
            lseek (tokens->items[1], offset);
        }
    } else if (strcmp (tokens->items[0], "write") == 0)
    {
        char *mode = get_mode_file_table (tokens->items[1]);

        if (strcmp (mode, "w") == 0 || strcmp (mode, "wr") == 0 || strcmp (mode, "rw") == 0)
        {
            int offset = get_offset_file_table (tokens->items[1]);
            int size = atoi (tokens->items[2]);
            DirectoryEntry *entry = get_file_size (bs, fp, tokens->items[1], FALSE);
            remove_quotes (tokens->items[3]);
            write_file (fp, bs, entry, size, offset, tokens->items[3]);
        } else
        {
            printf ("Error: cant write to file\n");
        }
    } else if (strcmp (tokens->items[0], "rm") == 0 && tokens->size == 2)
    {
        DirectoryEntry *entry = get_file_size (bs, fp, tokens->items[1], FALSE);
        if (entry != NULL)
        {
            remove_file (fp, bs, current_cluster_number, tokens->items[1]);
        }
    } else
    {
        printf ("Error: command not found or is incomplete\n");
    }
}

BootSector boot_parse (FILE *fp)
{
    static const int BIOS_PARAMETERS_OFFSET = 0x0b; // 11
    BootSector bs;
    fseek (fp, BIOS_PARAMETERS_OFFSET, SEEK_SET);
    fread (&bs, sizeof (BootSector), 1, fp);
    return bs;
}

void print_boot_sector (const BootSector *const bs)
{
    printf ("bytes per sector: %d\n", bs->bytes_per_sector);
    printf ("sectors per cluster: %d\n", bs->sectors_per_cluster);
    printf ("reserved cluster count: %d\n", bs->reserved_sector_count);
    printf ("number of FATs: %d\n", bs->number_of_fats);
    printf ("root entry count: %d\n", bs->root_entry_count);
    printf ("total sectors: %d\n", bs->total_sectors);
    printf ("FAT size: %d\n", bs->sectors_per_fat);
    printf ("root_dir_first_cluster: %d\n", bs->root_dir_first_cluster);
}

DirectoryEntry parse_directory_entry (FILE *fp, long offset)
{
    DirectoryEntry entry;
    fseek (fp, offset, SEEK_SET);
    fread (&entry, sizeof (DirectoryEntry), 1, fp);
    return entry;
}

// Combine the high and low word to get the first cluster of this file
uint32_t get_directory_entry_first_cluster (DirectoryEntry const *const dir)
{
    uint32_t cluster = dir->first_cluster_low_word;
    cluster |= ((uint32_t) dir->first_cluster_high_word) << 16;
    return cluster;
}

void print_directory_entry (const DirectoryEntry *const dir)
{
    printf ("name: %s\n", dir->short_file_name);
    printf ("attributes: 0x%x\n", dir->attributes);
    printf ("creation_time: %d\n", dir->creation_time);
    printf ("creation_date: %d\n", dir->creation_date);
    printf ("creation_time_tenth: %d\n", dir->creation_time_tenth);
    printf ("first sector: %u\n", get_directory_entry_first_cluster (dir));
}

// Return the number of sectors making up the FAT
int get_fat_size_in_sectors (BootSector const *const bs)
{
    return bs->number_of_fats * bs->sectors_per_fat;
}

// Return the first sector of the data section
int get_first_data_sector (BootSector const *const bs)
{
    return bs->reserved_sector_count + get_fat_size_in_sectors (bs);
}

// Return the offset in bytes for the start of this cluster
long get_cluster_offset (BootSector const *const bs, const int cluster)
{
    return (get_first_data_sector (bs) + (cluster - 2)
                                         * bs->sectors_per_cluster) * bs->bytes_per_sector;
}

// Return the offset in bytes for the start of the FAT
long get_fat_offset (BootSector const *const bs)
{
    return bs->reserved_sector_count * bs->bytes_per_sector;
}

// Lookup the current cluster in the FAT to find the next cluster
uint32_t get_next_cluster (FILE *fp, BootSector const *const bs, int current_cluster)
{
    uint32_t next_cluster;
    long offset = get_fat_offset (bs) + current_cluster * 4;
    fseek (fp, offset, SEEK_SET);
    fread (&next_cluster, 4, 1, fp);
    return next_cluster;
}

// Return 1 if this entry in the fat is the 'end of cluster' marker
// else Return 0 otherwise
int last_cluster_marker (uint32_t cluster)
{
    uint32_t masked = cluster & 0xfffffff;
    return masked >= 0xffffff8 && masked <= 0xfffffff;
}

void list_directory (BootSector const *bs, FILE *fp)
{
    current_cluster_number = current_directory;
    do
    {
        long current_entry_offset = get_cluster_offset (bs, (int) current_cluster_number);
        DirectoryEntry entry;
        int i;

        for (i = 0; i < 16; ++i)
        {
            entry = parse_directory_entry (fp, current_entry_offset + (i * 32));

            /* Break if there are no more entries left in the cluster */
            if (entry.short_file_name[0] == 0x0 || entry.short_file_name[0] == 0xE5)
            {
                break;
            } else if (entry.attributes == ATTR_LONG_NAME)
            {
                /* continue until we find a short-name entry */
                continue;
            }
            printf ("%s\n", entry.short_file_name);
        }
        current_cluster_number = get_next_cluster (fp, bs, (int) current_cluster_number);
    } while (!last_cluster_marker (current_cluster_number));

}

void change_directory (BootSector const *bs, const tokenlist *tokens, FILE *fp)
{
    long temp = previous_cluster_low;
    previous_cluster_low = current_directory;

    /* check if command is cd if true then go to root dir */
    if (tokens->size == 1)
    {
        current_directory = bs->root_dir_first_cluster;
        return;
    }

    char *dir_name = tokens->items[1];
    current_cluster_number = current_directory;

    do
    {
        long current_entry_offset = get_cluster_offset (bs, (int) current_cluster_number);
        DirectoryEntry entry;
        int i;

        for (i = 0; i < 16; ++i)
        {
            entry = parse_directory_entry (fp, current_entry_offset + (i * 32));
            remove_spaces (entry.short_file_name);
            if (entry.attributes != ATTR_DIRECTORY)
            {
                continue;
            } else if (strcmp (entry.short_file_name, dir_name) == 0)
            {
                if (entry.first_cluster_low_word == 0)
                {
                    current_directory = bs->root_dir_first_cluster;
                } else
                {
                    current_directory = entry.first_cluster_low_word;
                }
                return;
            }
        }
        current_cluster_number = get_next_cluster (fp, bs, (int) current_cluster_number);
    } while (!last_cluster_marker (current_cluster_number));

    /* dir not found so reset previous cluster low and print error */
    printf ("Error: directory does not exist\n");
    previous_cluster_low = temp;
}

DirectoryEntry *get_file_size (BootSector const *bs, FILE *fp,
                               char *file_name, int should_print)
{
    current_cluster_number = current_directory;
    do
    {
        long current_entry_offset = get_cluster_offset (bs, (int) current_cluster_number);
        DirectoryEntry entry;
        int i;
        for (i = 0; i < 16; ++i)
        {
            entry = parse_directory_entry (fp, current_entry_offset + (i * 32));
            /* Break if there are no more entries left in the cluster */
            if (entry.short_file_name[0] == 0x0 || entry.short_file_name[0] == 0xE5)
            {
                break;
            }

            remove_spaces (entry.short_file_name);
            if (strcmp (entry.short_file_name, file_name) == 0)
            {
                if (should_print)
                {
                    printf ("%d\n", entry.file_size);
                }
                return &entry;
            }
        }
        current_cluster_number = get_next_cluster (fp, bs, (int) current_cluster_number);
    } while (!last_cluster_marker (current_cluster_number));
    printf ("Error: file not found\n");
    return NULL;
}

void remove_spaces (char *s)
{
    char *ptr;
    ptr = strchr (s, ' ');
    if (ptr != NULL)
    {
        *ptr = '\0';
    }
}

// Lookup the current cluster in the FAT to find the next cluster
uint32_t read_fat (FILE *fp, BootSector *bs, int current_cluster)
{
    uint32_t next_cluster;
    long offset = get_fat_offset (bs) + current_cluster * 4;
    fseek (fp, offset, SEEK_SET);
    fread (&next_cluster, 4, 1, fp);
    return next_cluster;
}

void write_fat (FILE *fp, BootSector *bs, int cluster, uint32_t value)
{
    long offset = get_fat_offset (bs) + cluster * 4;
    fseek (fp, offset, SEEK_SET);
    fwrite (&value, sizeof (value), 1, fp);
}

// Return 1 if this entry in the fat is the 'end of cluster' marker
// Return 0 otherwise
int is_last_cluster_marker (uint32_t cluster)
{
    uint32_t masked = cluster & 0xfffffff;
    return masked >= 0xffffff8 && masked <= 0xfffffff;
}

// Returns the next free cluster number, returns -1 if there are none free
int get_next_free_cluster (FILE *fp, BootSector *bs, int search_start)
{
    const int fat_entries = get_fat_size_in_sectors (bs) / bs->sectors_per_cluster;
    int i;
    for (i = search_start; i < fat_entries; ++i)
    {
        if (read_fat (fp, bs, i) == 0x0)
        {
            return i;
        }
    }
    return -1;
}

int get_file_last_cluster (FILE *fp, BootSector *bs, int file_first_cluster)
{
    int current_cluster;
    int next_cluster = file_first_cluster;

    do
    {
        current_cluster = next_cluster;
        next_cluster = read_fat (fp, bs, current_cluster);
    } while (!is_last_cluster_marker (next_cluster));

    return current_cluster;
}

uint16_t get_current_date ()
{
    time_t t = time (NULL);
    struct tm tm = *localtime (&t);
    uint8_t year_bits = (tm.tm_year - 80) << (7 - ((int) (tm.tm_year - 80) + 1));
    uint8_t mon_bits = (tm.tm_mon + 1) << (4 - ((int) (tm.tm_mon + 1) + 1));
    uint8_t mday_bits = (tm.tm_mday) << (5 - ((int) (tm.tm_mday) + 1));
    return (year_bits << 4 | mon_bits) << 5 | mday_bits;

}

uint16_t get_current_time ()
{
    time_t t = time (NULL);
    struct tm tm = *localtime (&t);
    uint8_t hour_bits = (tm.tm_hour - 1) << (5 - ((int) (tm.tm_hour - 1) + 1));
    uint8_t minute_bits = (tm.tm_min - 1) << (6 - ((int) (tm.tm_min - 1) + 1));
    uint8_t seconds_bits = ((tm.tm_sec / 2) - 1) << (5 - ((int) ((tm.tm_sec / 2) - 1) + 1));

    return (hour_bits << 6 | minute_bits) << 5 | seconds_bits;
}

uint8_t get_current_miliseconds ()
{
    gettimeofday (&tv, NULL);
    return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

int create_empty_file (FILE *fp, BootSector *bs, int directory_first_cluster, char *filename, int is_directory)
{
    const int max_entries_in_cluster = (bs->sectors_per_cluster * bs->bytes_per_sector) / 32;
    int last_directory_cluster = get_file_last_cluster (fp, bs, directory_first_cluster);
    long last_directory_cluster_offset = get_cluster_offset (bs, last_directory_cluster);

    // Searches current directory cluster for available space to store the file information
    int directory_entry_idx;
    for (directory_entry_idx = 0;
         directory_entry_idx < max_entries_in_cluster;
         ++directory_entry_idx)
    {
        DirectoryEntry entry = parse_directory_entry
                (fp, last_directory_cluster_offset + (directory_entry_idx * 32));

        // Break if there are no more entries left in the cluster
        if (entry.short_file_name[0] == 0x0 || entry.short_file_name[0] == 0xE5)
        {
            break;
        } else if (strncmp (filename, (char *) &entry.short_file_name, 8) == 0)
        {
            fprintf (stderr, "File already exists!\n");
            return -1;
        }
    }

    // Finds next available cluster in the (whole) disk to store the file data
    int first_file_cluster = get_next_free_cluster (fp,
                                                    bs,
                                                    (int) bs->root_dir_first_cluster + 1);
    if (first_file_cluster == -1)
    {
        fprintf (stderr, "Not enough space create a new file!\n");
        return -2;
    }

    if (directory_entry_idx == max_entries_in_cluster)
    {
        // The last cluster for the directory information is full
        // we need to allocate a new cluster to store the new entry
        // find the next available space
        int new_directory_cluster = get_next_free_cluster (fp,
                                                           bs,
                                                           first_file_cluster + 1);
        if (new_directory_cluster == -1)
        {
            fprintf (stderr, "Not enough space create a new file!\n");
            return -2;
        }
        write_fat (fp, bs, last_directory_cluster, new_directory_cluster);
        write_fat (fp, bs, new_directory_cluster, 0xfffffff);
        last_directory_cluster = new_directory_cluster;
        directory_entry_idx = 0;
    }

    // Find the offset into the file of the data section of last directory info cluster
    const long new_directory_entry_offset = get_cluster_offset (bs,
                                                                last_directory_cluster)
                                            + directory_entry_idx * 32;
    DirectoryEntry new_entry;

    // Create the new directory entry of the new file
    strncpy((char *) new_entry.short_file_name, filename, 8);
    memset(new_entry.short_file_extension, ' ', sizeof (new_entry.short_file_extension));

    if (is_directory)
    {
        new_entry.attributes = 0x10;
    } else
    {
        new_entry.attributes = 0x0;
    }

    new_entry._unused_0 = 0;
    new_entry.creation_time_tenth = get_current_miliseconds ();
    new_entry.creation_time = get_current_time ();
    new_entry.creation_date = get_current_date ();
    new_entry.last_access_date = get_current_date ();
    new_entry.first_cluster_high_word = first_file_cluster >> 16;
    new_entry.write_time = get_current_time ();
    new_entry.write_date = get_current_date ();
    new_entry.first_cluster_low_word = first_file_cluster;
    new_entry.file_size = 0;

    // Write the new directory entry to the directory file
    fseek (fp, new_directory_entry_offset, SEEK_SET);
    fwrite (&new_entry, sizeof (DirectoryEntry), 1, fp);

    // Update FAT entry to show that this cluster is in-use
    // and it is the last cluster for this file
    write_fat (fp, bs, first_file_cluster, 0xffffffff);

    if (is_directory)
    {
        //Create . entry
        DirectoryEntry temp_entry = get_entry (bs, fp);
        strncpy((char *) new_entry.short_file_name, ".       ", 8);
        fseek (fp, get_cluster_offset (bs, first_file_cluster), SEEK_SET);
        fwrite (&new_entry, sizeof (DirectoryEntry), 1, fp);

        // Create .. entry
        fwrite (&temp_entry, sizeof (DirectoryEntry), 1, fp);

        // Create blank entry
        strncpy((char *) temp_entry.short_file_name, "        ", 8);
        temp_entry.short_file_name[0] = 0x0;
        temp_entry.attributes = 0;
        fwrite (&temp_entry, sizeof (DirectoryEntry), 1, fp);
    }
    fflush (fp);
    printf ("%screated: %ld bytes used, %ld bytes remaining\n", filename,
            total_size_of_files,
            total_size_remaining);
    return first_file_cluster;
}

void convert_string (char *str)
{
    /* string greater than 8 bits */
    if (strlen (str) > 8)
    {
        str[strlen (str) - 1] = '\0';
        return;
    }

    /* string less pad string less than 8 chars */
    char ch = ' ';
    while (strlen (str) < 8)
    {
        strncat(str, &ch, 1);
    }
}

long files_cluster_number (BootSector const *bs, FILE *fp, long current_offset)
{
    long temp = 0;
    current_cluster_number = current_offset;
    do
    {
        long current_entry_offset = get_cluster_offset (bs, (int) current_cluster_number);
        DirectoryEntry entry;
        int i;

        for (i = 0; i < 16; ++i)
        {
            entry = parse_directory_entry (fp, current_entry_offset + (i * 32));

            /* Break if there are no more entries left in the cluster */
            if (entry.short_file_name[0] == 0xE5)
            {
                continue;
            } else if (entry.attributes == ATTR_LONG_NAME)
            {
                /* continue until we find a short-name entry */
                continue;
            } else if (entry.attributes == ATTR_DIRECTORY && entry.short_file_name[0] != 0x2E)
            {
                temp += files_cluster_number (bs, fp, entry.first_cluster_low_word);
                continue;
            } else if (entry.attributes == ATTR_DIRECTORY)
            {
                continue;
            } else if (entry.short_file_name[0] == 0x0)
            {
                return temp;
            }
            temp += entry.file_size;
        }
        current_cluster_number = get_next_cluster (fp, bs, (int) current_cluster_number);
    } while (!last_cluster_marker (current_cluster_number));
    return temp;
}

long space_remaining (BootSector const *bs, FILE *fp, long cluster_number_of_files)
{
    int fat_clusters_used = (get_next_free_cluster (fp,
                                                    bs,
                                                    (int) bs->root_dir_first_cluster + 1) - 1);
    int fat_bytes_used = fat_clusters_used * 512;
    int fat_total_bytes = bs->sectors_per_fat * 512;
    return fat_total_bytes - fat_bytes_used;
}

DirectoryEntry get_entry (BootSector const *bs, FILE *fp)
{
    current_cluster_number = current_directory;
    long current_entry_offset = get_cluster_offset (bs, (int) current_cluster_number);
    DirectoryEntry entry;
    entry = parse_directory_entry (fp, current_entry_offset);
    if (entry.short_file_name[0] == 0x2E)
    {
        strncpy((char *) entry.short_file_name, "..      ", 8);
        return entry;
    } else
    {
        DirectoryEntry new_entry;
        new_entry.attributes = 0x10;
        strncpy((char *) new_entry.short_file_name, "..      ", 8);
        strncpy((char *) new_entry.short_file_extension, "   ", 3);
        new_entry._unused_0 = 0;
        new_entry.creation_time_tenth = 0;
        new_entry.creation_time = 0;
        new_entry.creation_date = 0;
        new_entry.last_access_date = 0;
        new_entry.first_cluster_high_word = 0;
        new_entry.write_time = 0;
        new_entry.write_date = 0;
        new_entry.first_cluster_low_word = 0;
        new_entry.file_size = 0;
        return new_entry;
    }
}

void add_open_list (char *file_name, char *mode)
{
    Open_File_Table *entry = (Open_File_Table *) malloc (sizeof (Open_File_Table));
    strncpy(entry->file_name, file_name, strlen (file_name));
    entry->offset = 0;
    strncpy(entry->mode, mode, strlen (mode));
    TAILQ_INSERT_TAIL(&head, entry, files);
}

void remove_item_open_list (char *file_name)
{
    Open_File_Table *table;
    TAILQ_FOREACH(table, &head, files)
    {
        /* check for matching item */
        if (strcmp (table->file_name, file_name) == 0)
        {
            TAILQ_REMOVE(&head, table, files);
            free (table);
            return;
        }
    }
}

void print_open_file_table ()
{
    Open_File_Table *table;
    TAILQ_FOREACH(table, &head, files)
    {
        printf ("Filename: %s | Offset: %ld | Mode: %s\n", table->file_name,
                table->offset,
                table->mode);
    }
}

int exist_in_file_table (char *file_name, int is_open)
{
    Open_File_Table *table;
    TAILQ_FOREACH(table, &head, files)
    {
        if (strcmp (table->file_name, file_name) == 0)
        {
            if (is_open)
            {
                printf ("Error: file already open\n");
            }
            return TRUE;
        }
    }
    return FALSE;
}

int valid_mode (char *mode)
{
    int result = FALSE;
    if (mode == NULL || (strcmp (mode, "r") != 0 && strcmp (mode, "w") != 0 &&
                         strcmp (mode, "wr") != 0 && strcmp (mode, "rw") != 0))
    {
        printf ("Error: mode not found\n");
    } else
    {
        result = TRUE;
    }
    return result;
}

char *read_file (FILE *fp, BootSector *bs, int file_size, int size, int offset)
{
    if (offset + size > file_size)
    {
        offset = (int) file_size - offset;
    }

    char *str;
    long byte_offset = get_cluster_offset (bs, (int) current_cluster_number);

    str = malloc (size);
    fseek (fp, offset + bs->bytes_per_sector + byte_offset, SEEK_SET);
    fread (str, size, 1, fp);

    return str;
}

void lseek (char *file_name, long offset)
{
    Open_File_Table *table;
    TAILQ_FOREACH(table, &head, files)
    {
        if (strcmp (table->file_name, file_name) == 0)
        {
            table->offset = offset;
            return;
        }
    }
}

int get_offset_file_table (char *file_name)
{
    Open_File_Table *table;
    int offset = -1;
    TAILQ_FOREACH(table, &head, files)
    {
        if (strcmp (table->file_name, file_name) == 0)
        {
            offset = (int) table->offset;
            break;
        }
    }
    return offset;
}

char *get_mode_file_table (char *file_name)
{
    Open_File_Table *table;
    TAILQ_FOREACH(table, &head, files)
    {
        if (strcmp (table->file_name, file_name) == 0)
        {
            return table->mode;
        }
    }
    return " ";
}

void write_file (FILE *fp, BootSector *bs, DirectoryEntry *entry, int size, int offset,
                 char *str)
{
    long byte_offset = get_cluster_offset (bs, (int) current_cluster_number);
    fseek (fp, offset + bs->bytes_per_sector + byte_offset, SEEK_SET);
    if (strlen (str) < size)
    {
        add_spaces (str, size);
    }
    fwrite (str, size, 1, fp);
}

int remove_file (FILE *fp, BootSector *bs, int directory_first_cluster_idx, char *filename)
{
    const int max_entries_in_cluster = (bs->sectors_per_cluster * bs->bytes_per_sector) / 32;
    int current_cluster_offset = get_cluster_offset (bs, directory_first_cluster_idx);
    int num_clusters = get_number_of_clusters (fp, bs, current_cluster_offset);
    int directory_entry_idx;
    int current_cluster_idx = directory_first_cluster_idx;

    DirectoryEntry entry;

    while (1)
    {
        for (directory_entry_idx = 0;
             directory_entry_idx < max_entries_in_cluster;
             ++directory_entry_idx)
        {
            entry = parse_directory_entry (fp, current_cluster_offset + (directory_entry_idx * 32));

            /* Break if no file is found in current cluster;
             * return error if no file found in last cluster
             */
            remove_spaces ((char *) &entry.short_file_name);
            if (entry.short_file_name[0] == 0x0)
            {
                break;
            }
                /* If file is found, locate its first cluster from the fat,
                 * and set as deleted (or as end of DIR)
                 */
            else if (strncmp (filename, (char *) &entry.short_file_name, 8) == 0)
            {
                if (entry.attributes & 0x10)
                {
                    fprintf (stderr, "Failed to delete: file is a directory\n");
                    return -1;
                }
                printf ("Found file at cluster %d entry %d\n", current_cluster_idx, directory_entry_idx);

                memset(entry.short_file_name, 0x0, sizeof (entry.short_file_name));
                entry.short_file_name[0] = 0xE5;

                fseek (fp, current_cluster_offset + (directory_entry_idx * 32), SEEK_SET);
                fwrite (&entry, sizeof (DirectoryEntry), 1, fp);

                current_cluster_idx = get_directory_entry_first_cluster (&entry);
                current_cluster_offset = get_cluster_offset (bs, current_cluster_idx);

                goto end;
            }
        }

        if (num_clusters == 1)
        {
            fprintf (stderr, "No File exists!\n");
            return -1;
        }
        num_clusters--;

        current_cluster_idx = read_fat (fp, bs, current_cluster_idx);
        current_cluster_offset = get_cluster_offset (bs, current_cluster_idx);
    }

    end:
    num_clusters = get_number_of_clusters (fp, bs, current_cluster_offset);
    int next_cluster_idx;

    // Go through each file cluster and update current FAT index as 0x0
    for (; num_clusters > 0; num_clusters--)
    {
        printf ("cluster nums: %d\n", num_clusters);
        next_cluster_idx = read_fat (fp, bs, current_cluster_idx);
        write_fat (fp, bs, current_cluster_idx, 0x0);
        current_cluster_idx = next_cluster_idx;
    }

    return 1;
}

int get_number_of_clusters (FILE *fp, BootSector *bs, int file_first_cluster)
{
    int current_cluster;
    int num = 0;
    int next_cluster = file_first_cluster;

    do
    {
        current_cluster = next_cluster;
        next_cluster = read_fat (fp, bs, current_cluster);
        num++;
    } while (!is_last_cluster_marker (next_cluster));

    return num;
}

void add_spaces (char *str, int num)
{
    char ch = ' ';
    while (strlen (str) < num)
    {
        strncat(str, &ch, 1);
    }
}

void remove_quotes (char *str)
{
    /* set up variables */
    char *dst = str;
    char *src = str;
    char ch;

    /* loop through str until "" are removed */
    while ((ch = *src++) != '\0')
    {
        if (ch == '\\')
        {
            *dst++ = ch;
            /* check for end of string */
            if ((ch = *src++) == '\0')
            {
                break;
            }
            *dst++ = ch;
        } else if (ch != '"')
        {
            *dst++ = ch;
        }
    }
    *dst = '\0';
}