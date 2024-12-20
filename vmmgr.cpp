#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <climits> 
using namespace std;

const int page_size = 256;
const int no_pages = 256;
const int no_frames = 256;
const int physical_size = no_frames * page_size;

int offset;
int page_no;
unsigned int addr;

struct Entry {
    int frameNumber;
    bool valid;

    /*constructor*/
    Entry() : frameNumber(-1), valid(false) {}
};

struct TLBEntry {
    int pageNumber;   
    int frameNumber; 
    bool valid;    
    int lastAccessTime;

    TLBEntry() : pageNumber(-1),frameNumber(-1), valid(false), lastAccessTime(INT_MAX) {}
};

class TLB {
private:
    TLBEntry entries[16];
    int next;   
    int currentTime;

public:

    TLB() {
        next = 0;  
        currentTime = 0;
    }
   
    int lookup(int pageNumber) {
        // Search the TLB for the page number
        for (int i = 0; i < 16; i++) {
            if (entries[i].valid && entries[i].pageNumber == pageNumber) {
                
                entries[i].lastAccessTime = currentTime++;
                return entries[i].frameNumber;  // TLB hit
            }
        }
        //TLB miss
        return -1; 
    }

   
    void addEntry(int pageNumber, int frameNumber) {
        // Check if the page is already in the TLB
        int lruIndex = -1;
        for (int i = 0; i < 16; i++) {
            if (entries[i].valid && entries[i].pageNumber == pageNumber) {
                // If the page is already in TLB, update it and return
                entries[i].frameNumber = frameNumber;
                entries[i].lastAccessTime = currentTime++;
          
                return;
            }
        }

        // If the page is not in the TLB, look for an invalid entry first
    for (int i = 0; i < 16; i++) {
        if (!entries[i].valid) {
            entries[i].pageNumber = pageNumber;
            entries[i].frameNumber = frameNumber;
            entries[i].valid = true;
            entries[i].lastAccessTime = currentTime++;
            return;
        }
    }

    // If no invalid entry, replace the LRU entry
    lruIndex = 0;
    for (int i = 1; i < 16; i++) {
        if (entries[i].lastAccessTime < entries[lruIndex].lastAccessTime) {
            lruIndex = i;
        }
    }

    // Replace the LRU entry with the new page
    entries[lruIndex].pageNumber = pageNumber;
    entries[lruIndex].frameNumber = frameNumber;
    entries[lruIndex].valid = true;
    entries[lruIndex].lastAccessTime = currentTime++;

    
        
    }
    
};


class MemoryManager {
private:
    Entry pageTable[no_pages];   
    bool frameUsage[no_frames];           
    char physicalMemory[physical_size]; 
    FILE *backingStore;                    

    int pageFaultCount;    
    int addressReferenceCount;

    int tlbhitCount;
    

public:
    MemoryManager(const char *backingStorePath) {
        // Initialize page table entries and frame usage
        for (int i = 0; i < no_frames; ++i) {
            pageTable[i] = Entry();
        }
        for (int i = 0; i < no_frames; ++i) {
            frameUsage[i] = false;
        }

        pageFaultCount = 0;
        addressReferenceCount = 0;
        tlbhitCount = 0;
        

        // Open the backing store file
        backingStore = fopen(backingStorePath, "rb");
        if (!backingStore) {
            perror("Failed to open backing store file.");
        }
    }

    ~MemoryManager() {
        if (backingStore) {
            fclose(backingStore);
        }
    }

    int translateAddress(int logicalAddress, int &physicalAddress,  signed char &byteValue, TLB &tlb) {
        offset = logicalAddress &  0x00FF;
     	page_no = (logicalAddress & 0xFF00) >> 8;
        

        addressReferenceCount++; 

        int frameNumber = tlb.lookup(page_no);

        // TLB miss
        if (frameNumber == -1) {  
            

            //Check page table for the frame number
            frameNumber = getFrameNumber(page_no);

            if (frameNumber == -1) { 
                //page fault
                cout << "Page fault for page: " << page_no << "\n";
                pageFaultCount++;  // Increment page fault count
                frameNumber = handlePageFault(page_no, tlb); // Load page from backing store
            }


            
        } else {
            // TLB hit
            tlbhitCount++;
        }

        physicalAddress = (frameNumber * page_size) + offset;
        byteValue = physicalMemory[physicalAddress];

        return physicalAddress;
    }

    int getFrameNumber(int pageNumber) {
        if (pageNumber < no_pages && pageTable[pageNumber].valid) {
            return pageTable[pageNumber].frameNumber;
        }
        return -1;
    }

    int handlePageFault(int pageNumber, TLB &tlb) {
        int frameNumber = findFreeFrame();
        if (frameNumber == -1) {
            perror("Out of physical memory!");
        }

        loadPageFromBackingStore(pageNumber, frameNumber);

        pageTable[pageNumber].frameNumber = frameNumber;
        pageTable[pageNumber].valid = true;

        frameUsage[frameNumber] = true;


        tlb.addEntry(pageNumber, frameNumber);

        return frameNumber;
    }

    void loadPageFromBackingStore(int pageNumber, int frameNumber) {
        // checking in backingstore 
        if (fseek(backingStore, pageNumber * page_size, SEEK_SET) != 0) {
            perror("Error seeking in backing store.");
        }

        // Read the page into the physical memory at the frame's location
        size_t bytesRead = fread(&physicalMemory[frameNumber * page_size], 1, page_size, backingStore);
        if (bytesRead != page_size) {
            perror("Error reading from backing store.");
        }
    }

    int findFreeFrame() {
        for (int i = 0; i < no_frames; ++i) {
            if (!frameUsage[i]) {
                return i;
            }
        }
        return -1;
    }

    //for debugging
    void displayPhysicalMemoryUsage() {
        cout << "Physical Memory Usage:\n";
        for (int i = 0; i < no_frames; ++i) {
            cout << (frameUsage[i] ? "1" : "0");
            if (i % 32 == 31) {
                cout << endl;
            }
        }
    }

    void printStatistics() {
        double pageFaultRate = (static_cast<double>(pageFaultCount) / addressReferenceCount) * 100.0;

        cout << "\nStatistics:\n";
        cout << "Total Logical Addresses Referenced: " << addressReferenceCount << endl;
        cout << "Page Faults: " << pageFaultCount << endl;
        cout << "Page-Fault Rate: " << pageFaultRate << endl;

        double hitRate = (static_cast<double>(tlbhitCount) / addressReferenceCount) * 100.0;
 

        cout << "\nTLB Statistics:\n";
        cout << "Total Address References: " << addressReferenceCount << endl;
        cout << "TLB Hits: " << tlbhitCount << endl;
        cout << "TLB Hit Rate: " << hitRate << endl;
    }
    
};




int main(int argc, char *argv[]){

        //constructors
        MemoryManager memoryManager("BACKING_STORE.bin");
        TLB tlb;

        ifstream addressFile(argv[1]);
        if (!addressFile) {
            perror("Failed to open addresses.txt file.");
        }

        int logicalAddress;
        //iterate through each address in file
        while (addressFile >> logicalAddress) {
            int physicalAddress;
            signed char byteValue;

            physicalAddress = memoryManager.translateAddress(logicalAddress, physicalAddress, byteValue, tlb);
            cout << "Logical Address: " << logicalAddress << " ";
            cout << "Physcical Address: " << physicalAddress << " ";
            cout << "Value: " << static_cast<int>(byteValue) << endl;
        }

        memoryManager.printStatistics();
       

	return 0;


}