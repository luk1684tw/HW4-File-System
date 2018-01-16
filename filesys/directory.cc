// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size)
{
    table = new DirectoryEntry[size];
	
	// MP4 mod tag
	memset(table, 0, sizeof(DirectoryEntry) * size);  // dummy operation to keep valgrind happy
	
    tableSize = size;
    for (int i = 0; i < tableSize; i++)
	    table[i].inUse = FALSE;
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{ 
    delete [] table;
} 

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void
Directory::FetchFrom(OpenFile *file)
{
    (void) file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void
Directory::WriteBack(OpenFile *file)
{
    (void) file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::FindIndex(char *name)
{
    for (int i = 0; i < tableSize; i++) {
        if (table[i].inUse&& !strncmp(table[i].name, name, FileNameMaxLen)) { 
	        //cout << "Name: " << table[i].name << endl;
            return i;
        }
    }
    return -1;		// name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::Find(char *name)
{
    //cout << "Directory::Find, name: " << name << endl;
    name++;
    char localName[256] = {0};
    char localID = 0;
    bool findNext = false;
    while (name[0] != '\0') {
        if (name[0] == '/') {
            findNext = true;
            break;
        }
        localName[localID++] = name[0];
        name++;
    }
    //cout << "localName: " << localName << endl;
    int i = FindIndex(localName);
    //cout << "i: " << i << endl;
    if (i != -1) {
        if (findNext) {
            OpenFile* nextDirectory = new OpenFile(table[i].sector);
            Directory* nextDir = new Directory(NumDirEntries);
            nextDir->FetchFrom(nextDirectory);
            int result = nextDir->Find(name);
            delete nextDirectory;
            delete nextDir;
            return result;
        } else {
            return table[i].sector;
        }
    } else {
        return -1;
    }
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool
Directory::Add(char *name, int newSector, char inType)
{ 
    if (Find(name) != -1)
	    return FALSE;

    char Path[256] = {0};
    char File[9] = {0};
    int len = strlen(name), slash, tmpID = 0;
    for (int i = len - 1; i >= 0; i--) {
        if (name[i] == '/') {
            slash = i;
            break;
        }
    }

    for (int i = slash+1; i < len; i++) {
        File[tmpID++] = name[i];
    }
    for (int i = 0; i < slash; i++) {
        Path[i] = name[i];
    }

    if (Path[0] != 0) {
        int sector = Find(Path);
        OpenFile* nextDirectory = new OpenFile(sector);
        Directory* nextDir = new Directory(NumDirEntries);
        nextDir->FetchFrom(nextDirectory);

        for (int i = 0; i < tableSize; i++) {
            if (!nextDir->table[i].inUse) {
                nextDir->table[i].inUse = true;
                strncpy(nextDir->table[i].name, File, FileNameMaxLen);
                nextDir->table[i].sector = newSector;
                nextDir->table[i].type = inType;
                nextDir->WriteBack(nextDirectory);

                delete nextDirectory;
                delete nextDir;
                return true;
            }
        }
    } else {
        for (int i = 0; i < tableSize; i++) {
            if (!table[i].inUse) {
                table[i].inUse = true;
                strncpy(table[i].name, File, FileNameMaxLen);
                table[i].sector = newSector;
                table[i].type = inType;
                return true;
            }
        }
    }
    return false;   // no space.  Fix when we have extensible files.
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool
Directory::Remove(char *name)
{
    if (Find(name) == -1)
        return false;
    
    char Path[256] = {0};
    char File[9] = {0};
    int len = strlen(name), tmpID = 0, slash;
    for (int i = len-1; i >= 0; i--) {
        if (name[i] == '/') {
            slash = i;
            break;
        }
    }

    for (int i = slash + 1; i < len; i++) {
        File[tmpID++] = name[i];
    }
    for (int i = 0; i < slash; i++) {
        Path[i] = name[i];
    }
    
    if (Path[0] != 0) {
        int sector = Find(Path);
        OpenFile* nextDirectory = new OpenFile(sector);
        Directory* nextDir = new Directory(NumDirEntries);
        nextDir->FetchFrom(nextDirectory);

        int id = nextDir->FindIndex(File);
        if (id == -1)
            return false;
        
        nextDir->table[id].inUse = false;
        nextDir->WriteBack(nextDirectory);
        delete nextDirectory;
        delete nextDir;
        return true;
    } else {
        int id = this->FindIndex(name);
        if (id == -1)
            return false;
        this->table[id].inUse = false;
        return true;
    }
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------

void
Directory::List()
{
    for (int i = 0; i < tableSize; i++) {
	    if (table[i].inUse)
	        printf("[Entry No.%d]: %s %c\n", i, table[i].name, table[i].type);
    }    
   return;
}

void Directory::RecursiveList(int depth)
{
    for (int i = 0; i < tableSize; i++) {
        if (table[i].inUse) {
            for (int j = 0; j < depth*8; j++)
                putchar(' ');
            printf("[Entry No.%d]: %s %c\n", i, table[i].name, table[i].type);
            if (table[i].type == 'D') {
                OpenFile* nextDirectory = new OpenFile(table[i].sector);
                Directory* nextDir = new Directory(NumDirEntries);
                nextDir->FetchFrom(nextDirectory);

                nextDir->RecursiveList(depth+1);
                delete nextDirectory;
                delete nextDir;
            }
        }
    }
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void
Directory::Print()
{ 
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (int i = 0; i < tableSize; i++) {
        if (table[i].inUse) {
            printf("Name: %s, Sector: %d\n", table[i].name, table[i].sector);
            hdr->FetchFrom(table[i].sector);
            hdr->Print();
	    }
    }
    printf("\n");
    delete hdr;
}
