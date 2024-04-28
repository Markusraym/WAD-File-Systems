#include <fuse.h>
#include "fuse.h"
#include "../libWad/Wad.h"
//Just copy pasted everything from Wad.cpp because I'm too lazy to pick and choose the ones I need
#include <iostream>
#include <fstream>
#include <sstream>
#include <stack>
#include <algorithm>
#include <cstring>
#include <set>
#include <unistd.h> //for get_current_dir_name()
#include <pwd.h> //for getpwnam
#include <grp.h> //for getgrnam


static int wadfs_getattr(const char *path, struct stat *stbuf) {
 memset(stbuf, 0, sizeof(struct stat));

  struct passwd *pwd = getpwnam("reptilian"); // Get user info for "reptilian"
  struct group *grp = getgrnam("reptilian"); // Get group info for "reptilian"


  //Check if path is a directory or file in your WAD library
  if (((Wad*)fuse_get_context()->private_data)->isDirectory(path)) 
  {
    stbuf->st_mode = S_IFDIR | 0777;
    stbuf->st_nlink = 2; 
    //So it says reptilian for ls -al 
    stbuf->st_uid = pwd->pw_uid; 
    stbuf->st_gid = grp->gr_gid; 
    return 0;
  } 
  else if (((Wad*)fuse_get_context()->private_data)->isContent(path)) 
  {
    stbuf->st_mode = S_IFREG | 0777; 
    stbuf->st_nlink = 1;
    stbuf->st_size = ((Wad*)fuse_get_context()->private_data)->getSize(path); 
    //So it says reptilian for ls -al 
    stbuf->st_uid = pwd->pw_uid;
    stbuf->st_gid = grp->gr_gid;
    return 0;
  }

  return -ENOENT; //path does not exist
}

static int wadfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    //Check if the path represents content in WAD library
    if (!((Wad*)fuse_get_context()->private_data)->isContent(path)) {
        return -ENOENT;  //path doesnt represent content
    }

    //use getContents to read the data from the WAD file
    int bytesRead = ((Wad*)fuse_get_context()->private_data)->getContents(path, buf, size, offset);
    if (bytesRead == -1) {
        //error handling if path does not represent content
        return -ENOENT;
    }

    //return the number of bytes read
    return bytesRead;
}

static int wadfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset; //Offset is not used in this case
    (void) fi;     //File info is not used in this case

    //add current and parent directory entries
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    //check if the path is a directory
    if (!((Wad*)fuse_get_context()->private_data)->isDirectory(path)) {
        return -ENOENT;  //path does not represent a directory
    }

    //Get the directory contents
    vector<string> directoryContents;
    int numEntries = ((Wad*)fuse_get_context()->private_data)->getDirectory(path, &directoryContents);
    if (numEntries == -1) {
        return -ENOENT;  //error in getting directory contents
    }

    //loopin through the directory contents and add each entry
    for (const string &entry : directoryContents) {
        filler(buf, entry.c_str(), NULL, 0);
    }

    return 0;
}


static int wadfs_mkdir(const char *path, mode_t mode) {
    (void) mode; //Mode is not used in this case

    //call createDirectory from WAD library
    try {
        ((Wad*)fuse_get_context()->private_data)->createDirectory(path);
    } catch (const std::exception& e) {
        return -EACCES;
    }

    return 0; //return 0 to indicate success
}


static int wadfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    (void) mode; //mode not be used
    (void) rdev; //Device type, not relevant for regular files

    //call createFile from your WAD library
    try {
        ((Wad*)fuse_get_context()->private_data)->createFile(path);
    } catch (const std::exception& e) {
        return -EACCES;
    }

    return 0; //return 0 to indicate success
}

static int wadfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    //(void) fi; // File info is not used in this case

    //call writeToFile from WAD library
    int bytesWritten = ((Wad*)fuse_get_context()->private_data)->writeToFile(path, buf, size, offset);
    if (bytesWritten == -1) {
        //Error handling if path does not represent content or other issues
        return -EIO; // Input/output error
    }

    // Return the number of bytes written
    return bytesWritten;
}

static struct fuse_operations wadfs_oper = {
    .getattr = wadfs_getattr,
    .mknod   = wadfs_mknod,
    .mkdir   = wadfs_mkdir,
    .read    = wadfs_read, 
    .write   = wadfs_write, 
    .readdir = wadfs_readdir,
};


int main(int argc, char *argv[]) {
    if (argc < 4) { //MAY NEED TO BE < 3. 
        std::cerr << "Usage: " << argv[0] << " -s <WAD file> <mount point>\n";
        exit(EXIT_SUCCESS);
    }

    std::string wadPath = argv[argc - 2]; //argv is 4 so 4 minus 2 equals 2, which would be somefile.wad
    if (wadPath.at(0) != '/') { //if the first char is not a forward slash, than its a relative path. We need to convert to absolute ones
        wadPath = std::string(get_current_dir_name()) + "/" + wadPath;
    }

    Wad* myWad = Wad::loadWad(wadPath);
    if (myWad == nullptr) {
        std::cerr << "Failed to load WAD file: " << wadPath << "\n";
        return 1;
    }

    //Swap them.
    argv[argc - 2] = argv[argc - 1];
    argc--;

    return fuse_main(argc, argv, &wadfs_oper, myWad); //return our operations struct, arg count, arg vector, and myWad object. 
}


/*
int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "Not enough arguments." << endl; 
        exit(EXIT_SUCCESS);
    }

    string wadPath = argv[argc - 2]; //argv is 4 so 4 minus 2 equals 2, which would be somefile.wad

    if (wadPath.at(0) != '/') { //if the first char is not a forward slash, than its a relative path. We need to convert to absolute ones
        wadPath == string(get_current_dir_name()) + "/" + wadPath; 
    }

    Wad* myWad = Wad::loadWad(wadPath);

    //here we're augmenting whats inside the command line args because FUSE doesn't know what a WAD file is, nor does it care.
    //For example, we'd pass in ./wadfs/wads -s someWadfile.wad /some/mount/directory, what fuse wants is flags (-s) and a mount directory.
    //So what we're doing is moving the somewadfile.wad command line argument by moving /some/mount/directory earlier? 
    //Not sure what the TA meant by this. But it's only going to see 3 arguments, and at position 3 where somewadfile.wad is, it'll instead
    //see /some/mount/directory, which what fuse actually cares about. 
    //Swap them. 
    argv[argc - 2] = argv[argc - 1]; 
    argc--;

    return fuse_main(argc, argv, &operations, myWad);
}   

//Also, we create the myWad object in main so we can use ((Wad*)fuse_get_context()->private_data)->getContents();
//We can call functions as if we had the original pointer, because you do, the pointer is just getting passed around. 
//This will be in our fuse functions. 
*/