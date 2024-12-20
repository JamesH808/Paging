for this assignment, i have a class for the TLB that search the TLB for a page number and add an entry using LRU method.
For the LRU method, I had a time stamped variable to determine which entries are to be replaced. 
my memry manager class takes care of coordinating translating the address, TLB look ups and handeling page faults and 
accessing the backing store bin file. in my main, i just call translate address from the memormanager, passing in a TLB and address.
Then I print the statistics for page fault rate and TLB hit rate. 