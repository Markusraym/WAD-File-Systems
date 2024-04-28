#include "Wad.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stack>
#include <algorithm>
#include <cstring>
#include <set>
using namespace std;


Wad* Wad::loadWad(const std::string &path) { //Load the wad file and read in information
    std::ifstream wadFile(path, std::ios::binary);
    if (!wadFile.is_open()) {
        throw std::runtime_error("Failed to open WAD file: " + path);
    }
    Wad* wad = new Wad(path);

    // Temporary variables to store file data. At first I needed temp vars and I would pass them to constructor at end but I changed everything to public
    std::string tempMagic;
    uint32_t tempNumOfDescriptors;
    uint32_t tempDescriptorOffset;
    
    // Read the magic number assuming it's 4 bytes
    char magicBuffer[5] = {};  //extra byte for null terminator
    wadFile.read(magicBuffer, 4);
    wad->magic = std::string(magicBuffer);

    //read the number of descriptors
    wadFile.read(reinterpret_cast<char*>(&tempNumOfDescriptors), sizeof(tempNumOfDescriptors));

    //Read the descriptor offset
    wadFile.read(reinterpret_cast<char*>(&tempDescriptorOffset), sizeof(tempDescriptorOffset));

    //seek forward descriptor offset to start reading descriptors
    wadFile.seekg(tempDescriptorOffset, std::ios::beg);

    //temporary data structure for building the tree
    std::stack<Node*> directoryStack;
    std::stack<std::string> pathStack; //To build paths for our map 
    Node* root = new Node(); //Root of n-ary tree
    root->file.isDirectory = true;
    root->file.name = "/"; //Root directory
    directoryStack.push(root);
    pathStack.push("/"); //Root path

    // Read descriptors
    for (uint32_t i = 0; i < tempNumOfDescriptors; ++i) {
        uint32_t elementOffset;
        uint32_t elementLength;
        char nameBuffer[9] = {}; //extra byte for null terminator

        wadFile.read(reinterpret_cast<char*>(&elementOffset), sizeof(elementOffset));
        wadFile.read(reinterpret_cast<char*>(&elementLength), sizeof(elementLength));
        wadFile.read(nameBuffer, 8); //names 8 bytes

        std::string elementName(nameBuffer);

        //determine if it's a file or directory based on the name and create a new Node
        bool isDirectory;
        //check for namespace marker
        if (elementName.find("_START") != std::string::npos || elementName.find("_END") != std::string::npos) {
            isDirectory = true;
        }
        //check for map marker (e.g., "E1M1")
        else if (elementName.size() == 4 && elementName[0] == 'E' && std::isdigit(elementName[1]) && elementName[2] == 'M' && std::isdigit(elementName[3])) {
            isDirectory = true;
        }
        else {
            isDirectory = false; //not a directory
        }

        File file = {elementName, elementOffset, elementLength, isDirectory};
        Node* newNode = new Node();
        newNode->file = file;

        //Logic to place newNode in the correct place in the tree
        Node* currentNode = directoryStack.top(); //Get the current directory node
        std::string currentPath = pathStack.top(); //Get the current path

        if (isDirectory) {
            currentNode->children.push_back(newNode); //Add the directory to the current node's children
            
            //This line causes duplicates like "/Gl/ad/os/os" but when fixing it it broke everything. I'm just gonna leave it
            std::string newPath = currentPath + wad->sanitizePath(elementName) + "/"; //Create the new path by combining current path with elementName
            wad->pathMap[newPath] = newNode; // Add to pathMap

            if (elementName.find("_END") != std::string::npos) {
                //If it's an end marker for a namespace, pop the stack
                directoryStack.pop();
                pathStack.pop();
            } else if (elementName.size() == 4 && elementName[0] == 'E' && std::isdigit(elementName[1]) && elementName[2] == 'M' && std::isdigit(elementName[3])) {
                //It's a map marker directory
                //For map markers, do not push onto the stack. Just set up for the next 10 files
                std::string mapMarkerPath = newPath; // Use the newPath for map marker path
                for (int j = 1; j <= 10 && (i + j) < tempNumOfDescriptors; ++j) {
                    uint32_t childElementOffset, childElementLength;
                    char childNameBuffer[9] = {};
                    wadFile.read(reinterpret_cast<char*>(&childElementOffset), sizeof(childElementOffset));
                    wadFile.read(reinterpret_cast<char*>(&childElementLength), sizeof(childElementLength));
                    wadFile.read(childNameBuffer, 8);

                    std::string childElementName(childNameBuffer);
                    File childFile = {childElementName, childElementOffset, childElementLength, false};
                    Node* childNode = new Node();

                    std::string childFilePath = mapMarkerPath + wad->sanitizePath(childElementName);
                    wad->pathMap[childFilePath] = childNode; //Add child file path to pathMap

                    childNode->file = childFile;
                    newNode->children.push_back(childNode); //Add files to the map marker directory
                }
                i += 9; //Increment the main loop counter for 10 files (including the current one)
            } else {
                //Regular namespace directory
                directoryStack.push(newNode);
                pathStack.push(newPath);
            }
        } else {
            //It's a file, add it to the current directory
            currentNode->children.push_back(newNode);
            std::string filePath = currentPath + elementName;
            wad->pathMap[filePath] = newNode; //Add to pathMap
            //I also noticed this is where the root is added to the pathMap. But I don't think it matters much. 
        }
    }

    auto rootIter = wad->pathMap.find("/");
    if (rootIter != wad->pathMap.end()) {
    rootIter->second->file.isDirectory = true; //Explicitly set the root as a directory. It somehow got set as false in the code above and im too lazy to find the issue
    }

    //Clean up and return the newly created Wad object
    wadFile.close();
    //Assign the temp vars to the wad object attributes
    wad->numOfDescriptors = tempNumOfDescriptors;
    wad->descriptorOffset = tempDescriptorOffset;
    wad->root = root; //assign the root of the tree to Wad object
    return wad;
}


Wad::Wad(const std::string &filePath) : path(filePath) {
    // The constructor now just stores the file path
    // All loading is done in loadWad
}


bool Wad::isContent(const std::string &path) { //Check if the path exists in the pathMap
    auto it = pathMap.find(path);
    if (it != pathMap.end()) {
        //If the path exists, check if the corresponding node is a file
        return !it->second->file.isDirectory;
    }

    //if the path doesn't exist in the pathMap, return false
    return false;
}


bool Wad::isDirectory(const std::string &path) {
    std::string modifiedPath = path;
    if (modifiedPath == "")
    {
        return false; 
    }
    //ensure the path ends with a forward slash
    if (modifiedPath.back() != '/') {
        modifiedPath += '/';
    }

    //check if the modified path exists in the pathMap
    auto it = pathMap.find(modifiedPath);
    if (it != pathMap.end()) {
        //if the path exists, check if the corresponding node is a directory
        return it->second->file.isDirectory;
    }

    //if the path doesn't exist in the pathMap, return false
    return false;
}



int Wad::getSize(const std::string &path) { //Returns the size in bytes of a file
    //First, check if the path represents content
    if (isContent(path)) {
        //The path is content, so find its corresponding node in pathMap
        auto it = pathMap.find(path);
        if (it != pathMap.end()) {
            //Return the size of the content
            return it->second->file.length;
        }
    }
    //If the path is not content or not found in pathMap, return -1
    return -1;
}



string Wad::getMagic() {
    return magic; //Return the magic member variable
}


int Wad::getContents(const std::string &path, char *buffer, int length, int offset) { //Gets the byte content from the file
    if (!isContent(path)) {
        //path does not represent content
        return -1;
    }

    // locate the node in pathMap
    auto it = pathMap.find(path);
    if (it == pathMap.end()) {
        //Path not found in pathMap
        return -1;
    }

    Node* node = it->second;

    //Open the WAD file
    std::ifstream wadFile(this->path, std::ios::binary); //this-> indicates the member variable path, not the passed in parameter
    if (!wadFile.is_open()) {
        return -1; //Failed to open file :(
    }

    //Calculate the actual read offset
    uint32_t readOffset = node->file.offset + offset;
    if (readOffset > node->file.offset + node->file.length) {
        //Offset is out of bounds
        wadFile.close();
        return 0; 
    }

    //Determine how many bytes to read
    int remainingLength = static_cast<int>(node->file.length) - offset;
    int bytesToRead = std::min(length, remainingLength);


    //seek to the start position and read the data into buffer
    wadFile.seekg(readOffset, std::ios::beg);
    wadFile.read(buffer, bytesToRead);

    //close the file and return the number of bytes read
    wadFile.close();
    return bytesToRead;
}



int Wad::getDirectory(const std::string &path, std::vector<std::string> *directory) { //Gets all the files in a given directory
    std::string modifiedPath = path;

    //Handle root directory separately
    Node* node;
    if (path == "/") {
        node = this->root; //use the root node directly
    } else {
        //Ensure the path ends with a forward slash
        if (!modifiedPath.empty() && modifiedPath.back() != '/') {
            modifiedPath += '/';
        }

        if (!isDirectory(modifiedPath)) {
            //Path does not represent a directory
            return -1;
        }
    
        //Locate the node in pathMap
        auto it = pathMap.find(modifiedPath);
        if (it == pathMap.end()) {
            //Path not found in pathMap
            //cout << "not found";
            return -1;
        }
        node = it->second;
    }

    //Clear existing contents of the directory vector
    directory->clear();
        
    //populate the vector with the names of each child, excluding _END files
    for (const auto& child : node->children) {
        if (!child->file.name.empty() && child->file.name.find("_END") == std::string::npos) {
            std::string nameWithoutSuffix = sanitizePath(child->file.name);
            directory->push_back(nameWithoutSuffix);
        }
    }
    //The code below is a bug fix. Adding files/dirs to the root doesnt show up for some reason, so I use my pathMap to look for missing stuff 
     if (path == "/") {
        std::set<std::string> uniqueNames(directory->begin(), directory->end());

        for (const auto& entry : pathMap) {
            //check if the entry is a direct child of root
            if (entry.first.front() == '/' && std::count(entry.first.begin(), entry.first.end(), '/') == 1 + entry.second->file.isDirectory) {
                //extract the name
                std::string name = entry.first.substr(1);
                if (entry.second->file.isDirectory) {
                    //For directories, remove the trailing slash
                    name = name.substr(0, name.length() - 1);
                }
                name = sanitizePath(name); //remove _START and _END if they exist

                //add the name if it's not already in the set
                if (uniqueNames.find(name) == uniqueNames.end()) {
                    directory->push_back(name);
                    uniqueNames.insert(name);
                }
            }
        }
    }

    //Return the number of elements added to the directory vector
    return directory->size();
}



void Wad::createDirectory(const std::string &path) { //Create a directory 
    //Extract the parent directory and new directory name from the path
    std::string parentPath;
    std::string newDirName;

    //Check if the last character is a slash
    bool hasTrailingSlash = path.back() == '/';

    //Count the number of forward slashes in the path
    int slashCount = std::count(path.begin(), path.end(), '/');

    //adjust the slash count if there is a trailing slash
    if (hasTrailingSlash) {
        //1 slash will be the trailing one, which we don't count for parent path purposes
        slashCount--;
    }

    //Check if the path is only one level deep. This checks if the parent directory is the root.
    if (path.front() == '/' && slashCount == 1) {
        parentPath = "/";
        //The new directory name is between the first slash and the end of the string (minus the trailing slash if it exists)
        newDirName = path.substr(1, path.length() - 1 - (hasTrailingSlash ? 1 : 0));
    } else {
        //Find the last slash that is not the trailing one
        size_t lastSlash = path.find_last_of('/', path.length() - (hasTrailingSlash ? 2 : 1));
        parentPath = path.substr(0, lastSlash + 1);
        //The new directory name is between the last slash and the end of the string (minus the trailing slash if it exists)
        newDirName = path.substr(lastSlash + 1, path.length() - lastSlash - 1 - (hasTrailingSlash ? 1 : 0));
    }

    if (newDirName.size() > 2) { //Because of _START, a name cant be more than 2 chars long. 
        return;
    }
 
    //Check if the parent directory exists
    if (!isDirectory(parentPath)) {
        //throw std::runtime_error("Parent directory does not exist");
        return;
    }

       //Check if the parent directory is a map marker
    if (isMapMarkerDirectory(parentPath)) { 
        //throw std::runtime_error("Parent directory is a map marker");
        return; 
    }

    //Locate the parent directory node in pathMap
    auto parentIt = pathMap.find(parentPath);
    if (parentIt == pathMap.end()) {
        //throw std::runtime_error("Parent directory not found in pathMap.");
        return;
    }
    Node* parentNode = parentIt->second;

    //Create a new node for the directory and add it to the n-ary tree
    Node* newDirNode = new Node();
    newDirNode->file.name = newDirName;
    newDirNode->file.isDirectory = true;
    newDirNode->file.length = 0;
    newDirNode->file.offset = 0;
    parentNode->children.push_back(newDirNode);

    //Update pathMap for the new directory
    std::string newDirFullPath = parentPath + newDirName + '/';
    pathMap[newDirFullPath] = newDirNode;
    //cout << "newDirFullPath: " << newDirFullPath << endl; 


    //Open the WAD file for reading and writing
    std::fstream wadFileStream(this->path, std::ios::in | std::ios::out | std::ios::binary);
    if (!wadFileStream.is_open()) {
        throw std::runtime_error("Failed to open WAD file for modification.");
        //return;
    }


    std::streampos ParentEndPosition;

    //Check if the parent directory is the root
    if (parentPath == "/") {
        // The ParentEndPosition is the end of the directory list
        // Which is the start of the directory list plus the size of all directory entries
        ParentEndPosition = this->descriptorOffset + (16 * this->numOfDescriptors);
    } else {
        //If it's not the root directory, find the position of the "_END" marker of parent directory 
        std::string modifiedPath = parentPath;
        modifiedPath.pop_back(); // Removes the "/" at the end
        //Find the last slash in the modified path
        size_t lastSlash2 = modifiedPath.find_last_of('/');
        //Extract the last directory name
        std::string lastDirName = modifiedPath.substr(lastSlash2 + 1);
        //Append "_END" to the last directory name
        std::string parentDescName = lastDirName + "_END";
        //If im putting F2 inside os, the F2_START and F2_INDEX needs to be right before os_END
        ParentEndPosition = findDescriptorPosition(wadFileStream, parentDescName, this->descriptorOffset);
        //std::cout << "Position of " << parentDescName << ": " << static_cast<long>(ParentEndPosition) << std::endl; //This gives the correct output 
    }

    // Read descriptors from F1_END marker to the end of the file. Copy everything from F1_END to the end of file into the buffer
    std::vector<char> descriptorBuffer; //Stores everything after the _END descriptor of parent directory
    wadFileStream.seekg(0, std::ios::end); //Go to the end of the file
    std::streampos EndPosition = wadFileStream.tellg(); //Get the end position
    //std::cout << "EndPosition in CreateDirectory: " << static_cast<long>(EndPosition) << std::endl; //correct output
    std::streampos sizeToRead = EndPosition - ParentEndPosition; //Calculate the size needed for the buffer 
    //std::cout << "Size to read in CreateDirectory: " << static_cast<long>(sizeToRead) << std::endl; //correct output
    descriptorBuffer.resize(static_cast<size_t>(sizeToRead)); //descriptorBuffer is resized to fit all the data from ParentEndPosition to the end of the file 
    wadFileStream.seekg(ParentEndPosition); //Maybe it should be parentEndPosition - 1? When I print the buffer it starts at the second byte. 
    wadFileStream.read(descriptorBuffer.data(), descriptorBuffer.size()); //read everything into our Descriptor buffer

    //determine the position to insert new directory descriptors (the first byte of the os_END descriptor)
    std::streampos insertPosition = ParentEndPosition;

    //Prepare new directory descriptors
    std::string startMarker = newDirName + "_START";
    std::string endMarker = newDirName + "_END";
    std::vector<char> newDescriptors(32, 0); // 2 descriptors, each 16 bytes long. initialize all bytes to 0
    //Copy the start marker name into the buffer, starting at byte 8
    std::memcpy(newDescriptors.data() + 8, startMarker.c_str(), startMarker.length());
    //Copy the end marker name into the buffer, starting at byte 24 (16 bytes for the first descriptor + 8 bytes offset for the second descriptor's name)
    std::memcpy(newDescriptors.data() + 24, endMarker.c_str(), endMarker.length());

    //Write new directory descriptors at the determined position
    wadFileStream.seekp(insertPosition);
    wadFileStream.write(newDescriptors.data(), newDescriptors.size());

    //Move the previously read descriptors forward to create space for new ones. the file's write position is already set to just after the new descriptors
    wadFileStream.write(descriptorBuffer.data(), descriptorBuffer.size());

    //Update the file header number of descriptors
    updateFileHeader(wadFileStream, 2);

    //Close the WAD file
    wadFileStream.close();
}

void Wad::createFile(const std::string &path) { //Create a file
    //cout << "createFile called on path: " << path << endl;
    //Extract the parent directory and new directory name from the path
    std::string parentPath;
    std::string newFileName;

    //Count the number of forward slashes in the path
    int slashCount = std::count(path.begin(), path.end(), '/');

    //Check if the path is only one level deep. This checks if the parent directory is the root. 
    if (path.front() == '/' && path.back() == '/' && slashCount == 2) {
        parentPath = "/";
        newFileName = path.substr(1, path.length() - 2); // Removing leading and trailing slashes
    } else {
        //Extract the parent directory and new directory name from the path
        size_t lastSlash = path.find_last_of('/');
        parentPath = path.substr(0, lastSlash);
        newFileName = path.substr(lastSlash + 1);
        parentPath += '/'; //Add the slash back for other operations
    }

    if (newFileName.size() > 8) { //a name cant be more than 8 chars long. 
        return;
    }
    //std::cout << "parentPath: " << parentPath << std::endl; //both come out correct
    //std::cout << "newFileName: " << newFileName << endl; 

    //check if fileName is in map marker format (e.g., 'E1M1')
    if (newFileName.length() == 4 &&
        std::isalpha(newFileName[0]) && std::isdigit(newFileName[1]) &&
        std::isalpha(newFileName[2]) && std::isdigit(newFileName[3])) {
        //The fileName is in map marker format, return early
        return;
    }

    //Check if the parent directory exists and is not a map marker
    if (!isDirectory(parentPath)) {
        //throw std::runtime_error("Parent directory does not exist");
        return;
    }

       //Check if the parent directory is a map marker
    if (isMapMarkerDirectory(parentPath)) { 
        //throw std::runtime_error("map marker!");
        return; 
    }

    //Locate the parent directory node in pathMap
    auto parentIt = pathMap.find(parentPath);
    if (parentIt == pathMap.end()) {
        //throw std::runtime_error("Parent directory not found in pathMap.");
        return;
    }
    //cout << "parent: " << parentIt->second->file.name << endl;
    Node* parentNode = parentIt->second;

    //Create a new node for the directory and add it to the n-ary tree
    Node* newDirNode = new Node();
    newDirNode->file.name = newFileName;
    newDirNode->file.isDirectory = false;
    newDirNode->file.length = 0;
    newDirNode->file.offset = 0;
    parentNode->children.push_back(newDirNode);

    //Update pathMap for the new directory
    std::string newFileFullPath = parentPath + newFileName;
    pathMap[newFileFullPath] = newDirNode;
    //cout << "newDirFullPath: " << newFileFullPath << endl; 


    //Open the WAD file for reading and writing
    std::fstream wadFileStream(this->path, std::ios::in | std::ios::out | std::ios::binary);
    if (!wadFileStream.is_open()) {
        throw std::runtime_error("Failed to open WAD file for modification.");
    }

    std::streampos ParentEndPosition;

    //Check if the parent directory is the root
    if (parentPath == "/") {
        //The ParentEndPosition is the end of the directory list
        //Which is the start of the directory list plus the size of all directory entries
        ParentEndPosition = this->descriptorOffset + (16 * this->numOfDescriptors);
    } else {
        //If it's not the root directory, find the position of the "_END" marker of parent directory 
        std::string modifiedPath = parentPath;
        modifiedPath.pop_back(); //Removes the "/" at the end
        //Find the last slash in the modified path
        size_t lastSlash2 = modifiedPath.find_last_of('/');
        //Extract the last directory name
        std::string lastDirName = modifiedPath.substr(lastSlash2 + 1);
        //Append "_END" to the last directory name
        std::string parentDescName = lastDirName + "_END";
        //If im putting "hello.txt" inside os, the hello.txt descriptor needs to be right before os_END
        ParentEndPosition = findDescriptorPosition(wadFileStream, parentDescName, this->descriptorOffset);
        //std::cout << "Position of " << parentDescName << ": " << static_cast<long>(ParentEndPosition) << std::endl; //This gives the correct output 
    }

    //Read descriptors from F1_END marker to the end of the file. Copy everything from F1_END to the end of file into the buffer
    std::vector<char> descriptorBuffer; //Stores everything after the _END descriptor of parent directory
    wadFileStream.seekg(0, std::ios::end); //Go to the end of the file
    std::streampos EndPosition = wadFileStream.tellg(); //Get the end position
    //std::cout << "EndPosition in CreateFile: " << static_cast<long>(EndPosition) << std::endl; //correct output
    std::streampos sizeToRead = EndPosition - ParentEndPosition; //Calculate the size needed for the buffer 
    //std::cout << "Size to read in CreateFile: " << static_cast<long>(sizeToRead) << std::endl; //correct output
    descriptorBuffer.resize(static_cast<size_t>(sizeToRead)); //descriptorBuffer is resized to fit all the data from ParentEndPosition to the end of the file 
    wadFileStream.seekg(ParentEndPosition); //Maybe it should be parentEndPosition - 1? When I print the buffer it starts at the second byte. 
    wadFileStream.read(descriptorBuffer.data(), descriptorBuffer.size()); //read everything into our Descriptor buffer

    //Determine the position to insert new directory descriptors (the first byte of the os_END descriptor)
    std::streampos insertPosition = ParentEndPosition;

    //Prepare new directory descriptors
    std::string startMarker = newFileName;
    std::vector<char> newDescriptor(16, 0); // 1 descriptor, 16 bytes long initialize all bytes to 0
    //Copy the name into the buffer, starting at byte 8
    std::memcpy(newDescriptor.data() + 8, startMarker.c_str(), startMarker.length());

    //Write new file descriptors at the determined position
    wadFileStream.seekp(insertPosition);
    wadFileStream.write(newDescriptor.data(), newDescriptor.size());

    //Move the previously read descriptors forward to create space for new ones. the file's write position is already set to just after the new descriptors
    wadFileStream.write(descriptorBuffer.data(), descriptorBuffer.size());

    //Update the file header number of descriptors
    updateFileHeader(wadFileStream, 1);

    //close the WAD file
    wadFileStream.close();
}

int Wad::writeToFile(const std::string &path, const char *buffer, int length, int offset) { //Write to the wad file

    if (!isContent(path)) {
        //Path does not represent content
        return -1;
    }

    //Locate the file node in pathmap, check if its length is 0.  
    auto it = pathMap.find(path);
    if (it == pathMap.end() || it->second->file.length != 0) {
        //file not found or its not empty
        return 0;
    }

    //Open WAD file for reading and writing
    std::fstream wadFileStream(this->path, std::ios::in | std::ios::out | std::ios::binary);
    if (!wadFileStream.is_open()) {
        //Error opening file
        return -1;
    }

    //Actual number of bytes to write
    uint32_t actualLength = length - offset;
    //Calculate new descriptor offset
    uint32_t newDescriptorOffset = descriptorOffset + actualLength;
    //std::cout << "new descriptor offset: " << std::hex << newDescriptorOffset << " (hex: " << descriptorOffset << ")" << std::dec << std::endl;

    //Read descriptors from ParentDirectory_END marker to the end of the file. Copy everything from ParentDirectory_END to the end of file into the buffer
    std::vector<char> descriptorBuffer; //Stores everything at the start of descriptors
    wadFileStream.seekg(0, std::ios::end); //go to the end of the file
    std::streampos EndPosition = wadFileStream.tellg(); //Get the end position/eof 
    std::streampos sizeToRead = EndPosition - static_cast<std::streamoff>(descriptorOffset);
    descriptorBuffer.resize(static_cast<size_t>(sizeToRead)); //descriptorBuffer is resized to fit all the data from ParentEndPosition to the end of the file 
    wadFileStream.seekg(descriptorOffset); //Maybe it should be descriptorOffset - 1? When I print the buffer it starts at the second byte in HxD. 

    wadFileStream.read(descriptorBuffer.data(), descriptorBuffer.size()); //read everything into our Descriptor buffer, should have the entire descriptor section

    //Write new lump data at old descriptor offset
    wadFileStream.seekp(this->descriptorOffset, std::ios::beg); //Go to the beginning of descriptor offset 
    wadFileStream.write(buffer + offset, actualLength); //Start copying from buffer[offset]

    //Move descriptor list forward. Once the writing from buffer is done, its already in position to write back in the old descriptor info
    wadFileStream.write(descriptorBuffer.data(), descriptorBuffer.size());

    //Update file's descriptor in the pathMap
    Node* fileNode = it->second;
    fileNode->file.offset = this->descriptorOffset; //Since the offset of the file is now in the original descriptor offset, set it to that value
    fileNode->file.length = actualLength;

    //Update WAD header 
    updateFileHeaderDescriptorOffset(wadFileStream, newDescriptorOffset);
    //Update file's descriptor in the WAD file
    std::string fileName = extractFileName(path);
    
    std::streampos descriptorPos = findDescriptorPosition(wadFileStream, fileName, this->descriptorOffset);
    wadFileStream.seekp(descriptorPos, std::ios::beg);
    wadFileStream.write(reinterpret_cast<const char*>(&fileNode->file.offset), sizeof(fileNode->file.offset));
    wadFileStream.write(reinterpret_cast<const char*>(&fileNode->file.length), sizeof(fileNode->file.length));
    
    //Close the WAD file
    wadFileStream.close();
    return actualLength; //Number of bytes written
}   

//MAKE SURE TO UPDATE IF IT CANT FIND IT, MAKE THE CALLING FUNCTION RETURN.
std::streampos Wad::findDescriptorPosition(std::fstream& wadFileStream, const std::string& descriptorName, uint32_t descriptorOffset) {
    wadFileStream.seekg(descriptorOffset, std::ios::beg);
    
    //cout << "descriptorName: " << descriptorName << endl;

    char nameBuffer[9]; //Name buffer (8 bytes + null terminator)
    uint32_t offset, length;
    while (wadFileStream.read(reinterpret_cast<char*>(&offset), sizeof(offset))
           && wadFileStream.read(reinterpret_cast<char*>(&length), sizeof(length))
           && wadFileStream.read(nameBuffer, 8)) {
        nameBuffer[8] = '\0'; //Null-terminate the string     
        if (descriptorName == nameBuffer) {
            auto position = static_cast<std::streamoff>(wadFileStream.tellg()) - 16; //Subtract 16 bytes to get the start of the descriptor
            //std::cout << "Found descriptor " << descriptorName << " at position: " << std::dec << position << " (hex: " << std::hex << position << ")" << std::endl;
            return position;
        }
        
    }
    return -1; 
    //std::cerr << "Failed to find descriptor: " << descriptorName << std::endl;
    //throw std::runtime_error("Descriptor not found.");
}


void Wad::updateFileHeader(std::fstream& wadFileStream, int additionalDescriptors) { //Updates the file header after creating files or directories
    //Update the number of descriptors
    this->numOfDescriptors += additionalDescriptors;

    //Seek to the position in the file where the number of descriptors is stored
    //Assuming it's stored at an offset of 4 bytes from the start of the file
    wadFileStream.seekp(4, std::ios::beg);

    //Write the updated number of descriptors
    wadFileStream.write(reinterpret_cast<const char*>(&this->numOfDescriptors), sizeof(this->numOfDescriptors));
}

void Wad::updateFileHeaderDescriptorOffset(std::fstream& wadFileStream, uint32_t additionalDescriptorOffset) { //Updates the file header after writing files
    //Update the descriptor offset
    this->descriptorOffset = additionalDescriptorOffset;

    //Seek to the position in the file where the number of descriptors is stored
    //Assuming it's stored at an offset of 8 bytes from the start of the file
    wadFileStream.seekp(8, std::ios::beg);

    //write the updated number of descriptors
    wadFileStream.write(reinterpret_cast<const char*>(&this->descriptorOffset), sizeof(this->descriptorOffset));
}


std::string Wad::extractFileName(const std::string& path) { //Gives me the file name when passed in an entire path 
    //remove the trailing slash if present, so if its like /Gl/ad/os/cake.jpg/ instead of /Gl/ad/os/cake.jpg
    std::string modifiedPath = path;
    if (modifiedPath.back() == '/') {
        modifiedPath.pop_back();
    }

    //find the last slash in the path
    size_t lastSlashPos = modifiedPath.find_last_of('/');

    //extract the file name (substring after the last slash)
    std::string fileName = modifiedPath.substr(lastSlashPos + 1);

    return fileName;
}


//Helper function: sanitizePath. This removes the _START and _END suffixes in my pathMap. 
std::string Wad::sanitizePath(const std::string& path) {
    if (path.size() >= 6 && path.substr(path.size() - 6) == "_START") {
        return path.substr(0, path.size() - 6);
    } else if (path.size() >= 4 && path.substr(path.size() - 4) == "_END") {
        return path.substr(0, path.size() - 4);
    }
    return path;
}



void printTree(const Node* node, const std::string& prefix) { //Helper/Test function to see if the tree is correct 
    if (!node) return;

    //Check if the node is a directory or file and print accordingly
    if (node->file.isDirectory) {
        //print directory name
        std::cout << prefix << "Directory: " << node->file.name << std::endl;
    } else {
        //print file name and length
        std::cout << prefix << "File: " << node->file.name << " (Length: " << node->file.length << ")" << std::endl;
    }

    //recursively (gross) print each child
    for (const auto& child : node->children) {
        printTree(child, prefix + "  ");
    }
    
}


bool Wad::isMapMarkerDirectory(const std::string &directoryName) { //Checks if a directory is a map marker. 
    //remove leading and trailing slashes from the directory path
    std::string directoryPath = directoryName;
    if (directoryPath.front() == '/') {
        directoryPath.erase(0, 1); //remove the first character
    }
    if (directoryPath.back() == '/') {
        directoryPath.pop_back(); //remove the last character
    }

    //cout << directoryPath << endl; //If its not 4 long, its not a map marker
    if (directoryPath.length() != 4) {
        return false;
    }

    //Check for the expected pattern: letter, digit, letter, digit
    if (std::isalpha(directoryPath[0]) && std::isdigit(directoryPath[1]) &&
        std::isalpha(directoryPath[2]) && std::isdigit(directoryPath[3])) {
        return true;
    }

    return false;
}
