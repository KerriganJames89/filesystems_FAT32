# FSU COP4610 Project 3
- FAT32 project

# Team Members
- Sam Anderson 
- Luis Corps
- James Kerrigan

# File listing
- Our project multiple contains files
    - main.c
    - fat.h
    - Makefile

# Makefile description
- Single dependency on fat.h
    - How to run how program:
        1) type `make` which will create the following:  `main.o` `project3.exe`

# Group Member Contribution
- **Sam Anderson**
    - Created github repo & Makefile
    - cd, size, open, close, lseek, read, ls, write
    - helped set up console interface (execute function) and research
    - provided utility functions for string padding
- **Luis Corps**
   - Part 8: mv FROM TO
   - Part 15: cp FILENAME TO
   - Part 6: creat FILENAME
   - cd, mkdir, execute, mv, cp
   - contributed with helper functions for mv and cp
   - error checking and testing
   - contributed to research and code optimization

- **James Kerrigan**
    - Part 2: Info
    - Part 6: creat FILENAME
    - Part 14: rm FILENAME
    - EXTRA CREDIT: rm DIRENTRY
    - contributed with helper functions and formulated getters; get_fat, get_cluster_offset, ect.
    - contributed by structuring the boot sector and directory entry information.
        
# Bugs
    - read function does not error check
    - open command will sometimes improperly store filename and mode
    - end of cluster entries may not always update correctly; rare case, may be related to data being overwritten incorrectly.
    - rm directory was working but started created problems in the data so it was disabled in the end
    - mv may create problems with directory moving but works fine with file
    
# Comments
      
    - Data is only marked for deletion when rm is used and the FAT will be freed. However, when creating a new file 
      or directory, previous data will be zeroed to prevent errors. Overall, reusing sectors seems to be working fine
      but debugging this stuff is a pain.
      
    - Time and date for entries was implemented, but apparently linprog gets mad at them.

    - write will append space to the end of "STRING" if size > "STRING"  

# Special considerations
- Github log will not accurately represent contribution since majority of code was shared through discord. Also made us of zoom by having meetings to review/modify members contributions
