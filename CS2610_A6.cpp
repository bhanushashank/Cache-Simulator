/*
    CS2610 Assignment 6 - Cache Simulator
    Team Members: 1) Akshith Sriram E.   (CS19B005)
                  2) B. Chetan Reddy     (CS19B012)
                  3) T. Bhanu Shashank   (CS19B043)
*/

#include <bits/stdc++.h>
using namespace std;

typedef long long ll;

// Cache Block
struct cacheBlock{
    ll tag;             // Tag of the memory location
    bool valid;         // Describes whether block contains valid data (or) not
    bool dirty;         // set to 1 when data in the cache is modified
    
    cacheBlock* next;   // Pointer to next cache block (in the linked list)
};

cacheBlock **Sets;  // To store cache blocks as an array of linked lists
char * fileName;    // Read file name (max length 100)

// Identifier names are self-explanatory
ll associativity;
ll replacementPolicy;
ll cacheHits = 0;
ll cacheMisses = 0;
ll blockCount;
ll setCount, setCountBits = 0;
ll cacheSize, blockSize, blockSizeBits = 0;
ll cacheAccesses = 0;
ll readAccesses = 0;
ll writeAccesses = 0;
ll compulsoryMisses = 0;
ll capacityMisses = 0;
ll conflictMisses = 0;
ll readMisses = 0;
ll writeMisses = 0;
ll dirtyBlocksEvicted = 0;
ll validBlockCount = 0;
ll setIndex, blockOffset, tagIndex;

bool readWrite;  // 0 for read op, 1 for write op

bool **visit;   // For tree based Pseudo-LRU replacement

map <pair<ll, ll>, bool> Map;   // To count conflict misses

// Create a set of size = No. of blocks (fully-associative cache)
//                      = Associativity (other cases)
cacheBlock * CreateSet(){

    cacheBlock * head = new cacheBlock;
    cacheBlock * temp = head;
    cacheBlock * last = head;

    int count = 1;
    // Create linked list of required size
    if(associativity){
        while(count++ < associativity){
            last = new cacheBlock;
            temp->next = last;
            temp = last;
        }
        last->next = NULL;
    } else {
        while(count++ < blockCount){
            last = new cacheBlock;
            temp->next = last;
            temp = last;
        }
        last->next = NULL;
    }
    return head;
}

// Create an array of sets.
void CreateCache(){
    Sets = new cacheBlock*[setCount];
    for(int i=0; i<setCount; i++) Sets[i] = CreateSet();
    
    cacheBlock * head;
    for(int i=0; i<setCount; i++){
        head = Sets[i];
        while(head != NULL){
            head->valid = false;
            head->dirty = false;
            head = head->next;
        }
    }
}

// Convert a Hexadecimal number to Decimal, return
ll HexToNum(string s){
    ll number = 0;
    for(int i=2; i< (int)s.size(); i++){
        number *= 16;
        if(s[i] >= '0' && s[i] <= '9')
            number += (s[i] - '0');
        else
            number += (s[i] - 'a') + 10;
    }
    return number;
}

// Assign no. of bits required for tag, set-index, block-offset
void GetBits(){
    int temp = blockSize;
    while(temp > 1){
        blockSizeBits++;
        temp /= 2;
    }

    temp = setCount;
    while(temp > 1){
        setCountBits++;
        temp /= 2;
    }
}

// Find the tag, set-index and block-offset of given address
void DecodeAddress(string addr){
    ll address = HexToNum(addr);
    blockOffset = address % blockSize;

    address /= blockSize;
    setIndex = address % setCount;

    address /= setCount;
    tagIndex = address;
}

// Random Replacement Policy
void Random(){
    cacheBlock *temp = Sets[setIndex];
    bool found = false;

    // Search for block
    while(temp != NULL){
        if(temp->valid && temp->tag == tagIndex){
            found = true;
            cacheHits++;
            if(readWrite) temp->dirty = 1; 
            return;
        }
        temp = temp->next;
    }

    // Block is not found
    cacheMisses++;  // It is a cache-miss
    
    // Check if the same address has been evicted before
    if(Map[make_pair(tagIndex, setIndex)] == 1){
        conflictMisses++;    // If the same address was evicted before, it is a conflict miss
        Map[make_pair(tagIndex, setIndex)] = 0;
    }
    else compulsoryMisses++; // If the same address was not evicted before, it is a compulsory miss

    // Search for invalid blocks (for victim selection)
    temp = Sets[setIndex];
    bool replaced = false;
    while(temp != NULL){
        if(!temp->valid){
            // Invalid block found, store the new address here
            temp->valid = true;
            temp->tag = tagIndex;
            temp->dirty = readWrite;
            if(readWrite) writeMisses += 1;
            else readMisses += 1;
            replaced = true;
            validBlockCount++;  // Invalid block is changed to valid block.
            return;
        }
        temp = temp->next;
    }

    // Check if the cache is completely filled (capacity misses should be considered 
    // only for fully-associative cache)
    if(validBlockCount == blockCount)
        capacityMisses++;
    
    // No invalid blocks found, choose a random index (valid block) to be evicted
    int index;
    if(associativity) index = rand() % associativity;
    else index = rand() % blockCount;

    // Traverse till that index
    int count = 0;
    temp = Sets[setIndex];
    while(count != index){
        temp = temp->next;
        count++;
    }
    Map[make_pair(temp->tag, setIndex)] = 1;

    if(temp -> dirty) dirtyBlocksEvicted++; // Count the no. of dirty blocks evicted.

    temp->valid = true;
    temp->tag = tagIndex;
    temp->dirty = readWrite;
    if(readWrite) writeMisses += 1;
    else readMisses += 1;
}

// LRU Replacement Policy
void LRU(){
    
    cacheBlock * head = Sets[setIndex];
    cacheBlock * temp = head;
    cacheBlock * current = temp->next;
    bool found = false;

    // Search for element in the cache
    // Check if the block is present at the head of linked list
    if(head->valid && head->tag == tagIndex){
        cacheHits++;
        if(readWrite){head->dirty = 1;}
        return;
    }

    // Search for the block in the remaining linked list
    while(current != NULL){
        if(current->valid && current->tag == tagIndex){
            // Cache Hit
            found = true;
            if(readWrite){current->dirty = 1;}
            cacheHits++;
            break;
        }
        current = current->next;
        temp = temp->next;
    }

    if(found){
        // If cache hit, bring the block to head
        temp->next = current->next;
        current->next = head;
        head = current;

        // Update head in the original sets array
        Sets[setIndex] = head;
        return;
    } 
    
    // Block is not found in the linked list
    cacheMisses++;

    // Check for conflict miss
    if(Map[make_pair(tagIndex, setIndex)] == 1){
        conflictMisses++;
        Map[make_pair(tagIndex, setIndex)] = 0;
    }
    else compulsoryMisses++;

    // Check if head is invalid, in which case we can load the block into head directly
    if(!head->valid){
        head->valid = true;
        head->tag = tagIndex;
        head->dirty = readWrite;
        validBlockCount++;
        if(!readWrite) readMisses++;
        else writeMisses++;
        return;
    }
    
    // Check for invalid blocks in the remaining linked list
    current = head->next;
    temp = head;
    bool replaced = false;
    while(current != NULL){
        if(!current->valid){
            // Invalid Block found, place the new block here
            current->valid = true;
            current->tag = tagIndex;
            current->dirty = readWrite;
            replaced = true;

            if(!readWrite) readMisses++;
            else writeMisses++;

            // Bring this block to the head of the linked list
            temp->next = current->next;
            current->next = head;
            head = current;

            // Update head of the original sets array.
            Sets[setIndex] = head;

            validBlockCount++;
            return;
        }
        current = current->next;
        temp = temp->next;
    }
    
    if(!replaced){
        // Check for capacity misses
        if(validBlockCount == blockCount)
            capacityMisses++;
        
        // Create a new block and place it as the head of the linked list
        cacheBlock * newHead = new cacheBlock;
        newHead->dirty = readWrite;
        newHead->tag = tagIndex;
        newHead->valid = true; 
        newHead->next = head;

        // Update head of the original sets array.
        Sets[setIndex] = newHead;
        head = newHead;

        if(readWrite) writeMisses++;
        else readMisses++;

        // Evict the last block of the linked list (as there are no invalid blocks)
        temp = head;
        while((temp->next)->next != NULL){
            temp = temp->next;
        }
        if((temp->next)->dirty && (temp->next)->next == NULL) {
            // One more dirty block is evicted
            dirtyBlocksEvicted++;
        }
        Map[make_pair((temp->next)->tag, setIndex)] = 1;
        temp->next = NULL;    
    }
    
}

// Pseudo-LRU Replacement Policy
void PseudoLRU(){
    
    cacheBlock * temp = Sets[setIndex];
    bool found = false;
    int count = associativity;
    if(!count) count = blockCount;

    // Check if the block is already present
    while(temp != NULL){
        if(temp->valid && temp->tag == tagIndex){
            // Cache Hit
            cacheHits++;
            
            if(readWrite) temp->dirty = 1;
            found = true;

            // Update Tree bits
            while(count != 0){
                int temp1 = count/2;
                if(count == 2*temp1) visit[setIndex][temp1] = 1;
                else visit[setIndex][temp1] = 0;  
                count = count/2;
            }
            return;
        }
        count ++;
        temp = temp->next;
    }
    
    // Block not found, fetch block into the location shown by tree bits
    cacheMisses++;
    
    if(Map[make_pair(tagIndex, setIndex)] == true){
        
        conflictMisses++;
        
        Map[make_pair(tagIndex, setIndex)] = false;
    }
    else compulsoryMisses++;
    
    int temp1 = 1;
    if(associativity){
        while(temp1 < associativity){
            temp1 = 2*temp1 + visit[setIndex][temp1];
            visit[setIndex][temp1] ^= 1;
        }
    } else {
        while(temp1 < blockCount){
            temp1 = 2*temp1 + visit[setIndex][temp1];
            visit[setIndex][temp1] ^= 1;
        }
    }
    int temp2 = associativity;
    if(!temp2) temp2 = blockCount;

    temp = Sets[setIndex];
    
    while(temp2 < temp1 && temp->next != NULL){
        temp = temp->next;
        temp2++;
    }
    
    // Check for capacity/conflict miss
    if(temp->valid){
        if(validBlockCount == blockCount){
            capacityMisses++;
        }
        if(temp->dirty) dirtyBlocksEvicted++;
        Map[make_pair(temp->tag, setIndex)] = 1;
    } else {
        validBlockCount++;
        temp->valid = true;
    }

    // Fetch block into that location.
    temp->tag = tagIndex;
    temp->dirty = readWrite;
    if(readWrite) writeMisses++;
    else readMisses++;
    
}

int main(){
    
    // Read Inputs
    cin >> cacheSize >> blockSize >> associativity;
    cin >> replacementPolicy;
    fileName = new char[100];
    cin >> fileName;
    
    // Number of blocks = (total cache size)/(size of each block)
    blockCount = cacheSize/blockSize;

    if(associativity) setCount = blockCount/associativity;  // No. of sets in set-associative (or) direct-mapped
    else setCount = 1;  // No. of sets = 1 for fully-associative cache

    CreateCache();      // Create cache with given size
    GetBits();          // Get no. of bits 

    visit = new bool*[setCount];
    if(associativity){
        for(int i=0; i<setCount; i++){
            visit[i] = new bool[associativity]; 
            memset(visit[i], 0, sizeof(visit[i]));
        }
    } else {
        for(int i=0; i<setCount; i++){
            visit[i] = new bool[blockCount];
            memset(visit[i], 0, sizeof(visit[i]));
        }
    }

    freopen(fileName, "r", stdin);
    
    string address;
    char input;

    switch (replacementPolicy)
    {
    case 0:
        // Random Replacement Policy
        while(cin >> address){
            
            cin >> input;
            if(input == 'r'){readWrite = 0; readAccesses++;}
            else {readWrite = 1; writeAccesses++;}
            cacheAccesses++;
            DecodeAddress(address);
            Random();  
        }
        break;
    case 1:
        // LRU Replacement Policy
        while(cin >> address){

            cin >> input;
            if(input == 'r'){readWrite = 0; readAccesses++;}
            else {readWrite = 1; writeAccesses++;}

            cacheAccesses++;
            DecodeAddress(address);
            LRU();   
        }
        break;
    case 2:
        // Pseudo-LRU Replacement Policy
        while(cin >> address){
            cin >> input;
            if(input == 'r'){readWrite = 0; readAccesses++;}
            else {readWrite = 1; writeAccesses++;}

            cacheAccesses++;
            DecodeAddress(address);
            PseudoLRU();   
        }
        break;
    default:
        cout << "Invalid Test Case, Correct it!" << endl;
        return -1;
    }
    fclose(stdin);

    freopen("output.txt", "w", stdout);
    cout << cacheSize << '\n';
    cout << blockSize << '\n';
    switch (associativity)
    {
    case 0:
        cout << "Fully-associative cache" << '\n';
        break;
    case 1:
        cout << "Direct-mapped cache" << '\n';
        break; 
    default:
        cout << "Set-associative cache" << '\n';
        break;
    }
    switch (replacementPolicy)
    {
    case 0:
        cout << "Random Replacement" << '\n';
        break;
    case 1:
        cout << "LRU Replacement" << '\n';
        break;
    case 2:
        cout << "Pseudo-LRU Replacement" << '\n';
        break;
    default:
        cout << "Invalid Test Case, correct it!" << '\n';
        return -1;
    }

    cout << cacheAccesses << '\n';
    cout << readAccesses << '\n';
    cout << writeAccesses << '\n';
    cout << cacheMisses << '\n';
    cout << compulsoryMisses << '\n';
    cout << capacityMisses << '\n';
    cout << conflictMisses << '\n';
    cout << readMisses << '\n';
    cout << writeMisses << '\n';
    cout << dirtyBlocksEvicted << '\n';
    fclose (stdout);
    return 0;
}