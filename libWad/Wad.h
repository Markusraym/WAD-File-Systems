#ifndef WAD_H
#define WAD_H

#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

// File structure
struct File {
    std::string name;
    uint32_t offset;
    uint32_t length;
    bool isDirectory;
    // Add other attributes here
};

// Node for the n-ary tree
class Node {
public:
    File file;
    std::vector<Node*> children;
    // Constructor, Destructor, and other methods
    ~Node() {
        for (auto child : children) {
            delete child; // Recursively delete child nodes
        }
    }
};

// Wad class
class Wad {
public:
    //Wad(const std::string &path); // Constructor to load a WAD file, may not be needed
    //explicit Wad(const std::string &filePath);
    static Wad* loadWad(const std::string &path); // Static method to load WAD file
    //~Wad(); // Destructor
    ~Wad() {
        delete root; // Delete the root node, which triggers recursive deletion of the tree
    }

    // Member functions as per project requirements
    bool isContent(const std::string &path); //Returns true if path represents content (data), and false otherwise.
    bool isDirectory(const std::string &path); //Returns true if path represents a directory, and false otherwise.
    int getSize(const std::string &path); //If path represents content, returns the number of bytes in its data; otherwise, returns -1. 
    string getMagic(); //Returns the magic for this WAD data. 
    int getContents(const string &path, char *buffer, int length, int offset = 0); // int getContents(const std::string &path, char *buffer, int length, int offset = 0); /If path represents content, copies as many bytes as are available, up to length, of content's data into the preexisting buffer. 
    int getDirectory(const string &path, vector<string> *directory); //If path represents a directory, places entries for immediately contained elements in directory.
    void createDirectory(const std::string &path); //path includes the name of the new directory to be created. If given a valid path, creates a new directory
    void createFile(const std::string &path); //path includes the name of the new file to be created. If given a valid path, creates an empty file at path
    int writeToFile(const std::string &path, const char *buffer, int length, int offset = 0); //writes the information from buffer to the .wad file
    std::string sanitizePath(const std::string& path); //Removes the _START and _END suffixes from my pathmap
    void updateFileHeader(std::fstream& wadFileStream, int additionalDescriptors); //Updates the number of descriptors in the WAD file
    void updateFileHeaderDescriptorOffset(std::fstream& wadFileStream, uint32_t additionalDescriptorOffset); //Updates the discriptor offset in the WAD file
    //std::string getParentDirectoryName(const std::string& path); // Helper function to extract the parent directory name from a path
    std::streampos findDescriptorPosition(std::fstream& wadFileStream, const std::string& descriptorName, uint32_t descriptorOffset); //finds the position of a specific descriptor in the WAD file
    std::string extractFileName(const std::string& path); //Gives me the file name when passed in an entire path 
    bool isMapMarkerDirectory(const std::string &directoryName); //Tells me if a directory is a map marker given it's name

    Wad(const std::string &filePath);
    std::unordered_map<std::string, Node*> pathMap; // Path lookup map
    Node* root; // Root of the n-ary tree
    std::string path; // Store the path of the WAD file
    std::string magic;  // To store the magic number
    uint32_t numOfDescriptors;  // To store the number of descriptors
    uint32_t descriptorOffset;  // To store the descriptor offset

};

void printTree(const Node* node, const std::string& prefix = ""); //helper function to print the tree

#endif // WAD_H
