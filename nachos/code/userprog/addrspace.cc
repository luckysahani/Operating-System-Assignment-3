// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
#include <stdio.h>
#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
//#include <stdlib.h>
//using namespace std;
//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable, char * filename)
{
    NoffHeader noffH;
    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;

    char_exec = filename;
//    exec_ptr = executable;
//    DEBUG('p',"In exec_ptr \n");
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

//    ASSERT(numPages+numPagesAllocated <= NumPhysPages);		// check we're not trying
										// to run anything too big --
										// at least until we have
										// virtual memory

    DEBUG('z', "Initializing address space, num pages %d, size %d\n", 
					numPages, noffH.code.size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;
	pageTable[i].physicalPage = -1;
	pageTable[i].valid = FALSE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
	pageTable[i].shared= FALSE;
    pageTable[i].backedup=FALSE;				// a separate page, we could set its 
					// pages to be read-only
    }

    initbackup(size);
   //  if (noffH.code.size > 0) {
   //      DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			// noffH.code.virtualAddr, noffH.code.size);
   //      vpn = noffH.code.virtualAddr/PageSize;
   //      offset = noffH.code.virtualAddr%PageSize;
   //      entry = &pageTable[vpn];
   //      pageFrame = 0;
   //      executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
			// noffH.code.size, noffH.code.inFileAddr);
   //  }
   //  if (noffH.initData.size > 0) {
   //      DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			// noffH.initData.virtualAddr, noffH.initData.size);
   //      vpn = noffH.initData.virtualAddr/PageSize;
   //      offset = noffH.initData.virtualAddr%PageSize;
   //      entry = &pageTable[vpn];
   //      pageFrame = entry->physicalPage;
   //      executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
			// noffH.initData.size, noffH.initData.inFileAddr);
   //  }

}



void
AddrSpace::initbackup(int size){
    backup = new char[size];
}

int
AddrSpace::shm_all(int size_shared){
    DEBUG('y',"In shared memory \n");
    int page_shared = divRoundUp(size_shared, PageSize);
    unsigned int new_numPages = numPages + page_shared;
    //DEBUG('y',"Number of pages shared %d",page_shared);
    TranslationEntry* pageTable_new = new TranslationEntry[new_numPages];
    unsigned int i=0;
    
    int next_page;
    for (i = numPages; i < new_numPages; i++) {
        pageTable_new[i].virtualPage = i;
        if(numPagesAllocated == NumPhysPages){
            next_page=replace(-1);
        }
        else if(free_page_count!=0){
            next_page = (int)(pagesfree->Remove());
            free_page_count--;
            numPagesAllocated++;
        }
        else{                
            next_page = next_phys_index;
            next_phys_index++;
            numPagesAllocated++;
        }
        pageTable_new[i].physicalPage = next_page;
        physpid[next_page] = currentThread->GetPID();
        physvpn[next_page] = i;
        bzero(&machine->mainMemory[next_page*PageSize],PageSize);
        pageTable_new[i].valid = TRUE;
        pageTable_new[i].use = FALSE;
        pageTable_new[i].dirty = FALSE;
        pageTable_new[i].readOnly = FALSE;    // if the code segment was entirely on
        pageTable_new[i].shared = TRUE;
        pageTable_new[i].backedup = TRUE;
        lruclk[next_page] = -1;
    }

    for (i = 0; i < numPages; i++) {
        pageTable_new[i].virtualPage = pageTable[i].virtualPage;
        pageTable_new[i].physicalPage = pageTable[i].physicalPage;
        pageTable_new[i].valid = pageTable[i].valid;
        pageTable_new[i].use = pageTable[i].use;
        pageTable_new[i].dirty = pageTable[i].dirty;
        pageTable_new[i].readOnly = pageTable[i].readOnly;
        pageTable_new[i].shared = pageTable[i].shared;    // if the code segment was entirely on
        pageTable_new[i].backedup = pageTable[i].backedup;
    }

    int retvalue=numPages;
    DEBUG('z',"Here are tou in shm allocate %d\n",numPages);
    delete pageTable;
    numPages = new_numPages;
    pageTable = pageTable_new;
//    DEBUG('k',"Numpages after shmall %d\n",pageTable[14].shared);    
    RestoreState();
//    numPagesAllocated+=page_shared;
    return retvalue*PageSize;
}



//----------------------------------------------------------------------
// AddrSpace::AddrSpace (AddrSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

AddrSpace::AddrSpace(AddrSpace *parentSpace)
{
    numPages = parentSpace->GetNumPages();
    unsigned i, size = numPages * PageSize;

//    ASSERT(numPages+numPagesAllocated <= NumPhysPages);                // check we're not trying
                                                                                // to run anything too big --
                                                                                // at least until we have
                                                                // virtual memory
    char_exec=parentSpace->char_exec;
    DEBUG('z', "Initializing address space, num pages %d, size %d\n",
                                        numPages, size);
    // first, set up the translation
    TranslationEntry* parentPageTable = parentSpace->GetPageTable();
    pageTable = new TranslationEntry[numPages];
    int j=0;
 //   DEBUG('k',"Hello %d\n",parentPaTable[14].shared);    
    int next_page,next_page_parent;
   // for (i = 0; i < numPages; i++) {

    //     pageTable[i].virtualPage = i;
    //     if(parentPageTable[i].shared==FALSE && parentPageTable[i].valid){
    //         if(numPagesAllocated == NumPhysPages){
    //             next_page=replace(0,0);
    //         }
    //         else if(free_page_count!=0){
    //             next_page = (int)(pagesfree->Remove());
    //             free_page_count--;
    //             numPagesAllocated++;
    //         }
    //         else{
                
    //             next_page = next_phys_index;
    //             next_phys_index++;
    //             numPagesAllocated++;
    //         }
    //         pageTable[i].physicalPage = next_page;
    //         physpid[next_page] = 1;
    //         //DEBUG('w', "I am here for vpn%d and numPagesAllocated%d\n",i,numPagesAllocated);
    //         DEBUG('w', "I am here for pid%d\n",1);
    //         physvpn[next_page] = i;
    //         next_page_parent=parentPageTable[i].physicalPage;
    //         j++;
    //         for(int z=0;z<PageSize;z++){
    //             machine->mainMemory[PageSize*next_page+z] = machine->mainMemory[next_page_parent*PageSize+z];
    //         }
    //     }
    //     else{
    //         pageTable[i].physicalPage=parentPageTable[i].physicalPage;
    // //        DEBUG('k',"Hello %d\n",i);
    //     }
        // pageTable[i].valid = parentPageTable[i].valid;
        // pageTable[i].use = parentPageTable[i].use;
        // pageTable[i].dirty = parentPageTable[i].dirty;
        // pageTable[i].readOnly = parentPageTable[i].readOnly;    // if the code segment was entirely on
        // pageTable[i].shared = parentPageTable[i].shared;                                            // a separate page, we could set its
        // pageTable[i].backedup= parentPageTable[i].backedup;                                            // pages to be read-only
 //   }
    initbackup(size);
    

   // DEBUG('w', "I am here for vpn after fork\n");
    
    // Copy the contents
    // unsigned startAddrParent = parentPageTable[0].physicalPage*PageSize;
    // unsigned startAddrChild = numPagesAllocated*PageSize;
    
    // j=0;
    
/*    for (i=0; i<numPages; i++) {
        if(!parentPageTable[i].shared && parentPageTable[i].valid){
            for(int k=0; k<PageSize; k++){
                machine->mainMemory[startAddrChild+(j*PageSize+k)] = machine->mainMemory[startAddrParent+(i*PageSize+k)];
            }
            j++;
        }
    }
*/
}





//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    DEBUG('j', "Getting outof thread\n");
    for(unsigned int i=0;i<numPages;i++){
        if(pageTable[i].valid && !pageTable[i].shared){    
            pagesfree->Append((void*)(pageTable[i].physicalPage));
            numPagesAllocated--;
        }
    }
   delete pageTable;
}



//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

unsigned
AddrSpace::GetNumPages()
{
   return numPages;
}

TranslationEntry*
AddrSpace::GetPageTable()
{
   return pageTable;
}


int
AddrSpace::handle_PFE(int vpn){

    exec_ptr = fileSystem->Open(char_exec);
    NoffHeader noffH;
    DEBUG('p',"Handling page faults inside handle_PFE\n");
    int next_page;
//    DEBUG('y',"I am i free world with alloc %d\n",numPagesAllocated);

    if(numPagesAllocated == NumPhysPages){
        DEBUG('o',"In the replacement for pid %d and vpn %d\n",currentThread->GetPID(),vpn);
    //    DEBUG('y',"In the handle_PFE for pid %d and vpn %d\n",currentThread->GetPID(),vpn);
        next_page=replace(-1);
    }
    else if(free_page_count!=0){
        next_page = (int)(pagesfree->Remove());
        free_page_count--;
        numPagesAllocated++;
        maintain(next_page);
    }
    else{        
        next_page = next_phys_index;
        next_phys_index++;
        numPagesAllocated++;
        maintain(next_page);
    }
    pageTable[vpn].valid = TRUE;
    pageTable[vpn].physicalPage = next_page;
    physpid[next_page] = currentThread->GetPID();
    physvpn[next_page] = vpn;
 //   maintain(next_page);

    DEBUG('z',"Handling page faults at the end handle_PFE %d\n",next_page);
    bzero(&machine->mainMemory[next_page*PageSize], PageSize);


    if(!pageTable[vpn].backedup){
        exec_ptr->ReadAt((char *)&noffH, sizeof(noffH), 0);

        if (noffH.code.size > 0) {
            int vpn_file = noffH.code.virtualAddr/PageSize;
            int offset_file = noffH.code.virtualAddr%PageSize;
            if(!(vpn_file > vpn || (noffH.code.virtualAddr+noffH.code.size)/PageSize < vpn))
            {
                if(vpn_file<=vpn){
                    int offset=0;
                    int copysize;
                    if((noffH.code.virtualAddr+noffH.code.size)/PageSize > vpn)
                    {
                        DEBUG('p', "Initializing code segment, at 0x%x, size %d\n",noffH.code.virtualAddr, pageTable[vpn].physicalPage);                       
                        exec_ptr->ReadAt(&(machine->mainMemory[next_page * PageSize + offset]),PageSize, noffH.code.inFileAddr+PageSize*vpn);
                    }
                    else{
                        exec_ptr->ReadAt(&(machine->mainMemory[next_page * PageSize + offset]),noffH.code.size - PageSize*vpn, noffH.code.inFileAddr+PageSize*vpn);
                    }
                }
                
            }
        }
        if(noffH.initData.size > 0) {
            DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
                noffH.initData.virtualAddr, noffH.initData.size);
            int vpn_file = noffH.initData.virtualAddr/PageSize;
            int offset_file = noffH.initData.virtualAddr%PageSize;
            if(!(vpn_file > vpn || (noffH.initData.virtualAddr+noffH.initData.size)/PageSize < vpn))
            {
                if(vpn_file<vpn){
                    int offset=0;
                    if((noffH.initData.virtualAddr+noffH.initData.size)/PageSize > vpn)
                    {
                        exec_ptr->ReadAt(&(machine->mainMemory[next_page * PageSize + offset]),PageSize, noffH.initData.inFileAddr + PageSize*vpn);
                    }
                    else{
                         exec_ptr->ReadAt(&(machine->mainMemory[next_page * PageSize + offset]),noffH.initData.virtualAddr+noffH.initData.size - PageSize*vpn,
                         noffH.initData.inFileAddr + PageSize*vpn- noffH.initData.virtualAddr);
                    }
                }
                else{
                    //    DEBUG('y',"Here at vpn %d with vpnfile %d\n ", vpn, vpn_file);
                    if(offset_file+ noffH.initData.size > PageSize){
                        exec_ptr->ReadAt(&(machine->mainMemory[next_page * PageSize + offset_file]),PageSize - offset_file, noffH.initData.inFileAddr);
                    }
                    else{
                        exec_ptr->ReadAt(&(machine->mainMemory[next_page * PageSize + offset_file]),noffH.initData.size , noffH.initData.inFileAddr);
                    } 
                }
                
            }
        }
        pageTable[vpn].backedup=TRUE;
    }
    else{
//        DEBUG('y', "I am here for vpn%d and numPagesAllocated%d\n",vpn,numPagesAllocated);
        DEBUG('o', "Here at the backup Array for %d %d\n",currentThread->GetPID(),vpn);
        for(int i=0;i<PageSize;i++){
            machine->mainMemory[next_page*PageSize+i] = backup[vpn*PageSize+i];
        }
    }
//    numPagesAllocated++;
    delete exec_ptr;
        //DEBUG('y',"In the handle_PFE for pid %d and vpn %d\n",currentThread->GetPID(),vpn);
    return vpn;
    // else{

    // }

}


void AddrSpace::Manage(int pid, AddrSpace *parentSpace ){
    int next_page,next_page_parent;
    TranslationEntry * parentPageTable = parentSpace->pageTable;
    int j=0;
    for (int i = 0; i < numPages; i++) {

        pageTable[i].virtualPage = i;
        if(parentPageTable[i].shared==FALSE && parentPageTable[i].valid){

            next_page_parent=parentPageTable[i].physicalPage;
            if(algo==2){
                lru->bringtotop(physlru[next_page_parent]);
            }
            else if(algo == 3){
                lruclk[next_page_parent] = 1;
            }


            if(numPagesAllocated == NumPhysPages){
                next_page=replace(parentPageTable[i].physicalPage);
            }
            else if(free_page_count!=0){
                next_page = (int)(pagesfree->Remove());
                free_page_count--;
                numPagesAllocated++;
                maintain(next_page);
            }
            else{                
                next_page = next_phys_index;
                next_phys_index++;
                numPagesAllocated++;
                maintain(next_page);
            }
            pageTable[i].physicalPage = next_page;
            physpid[next_page] = pid;
            //DEBUG('w', "I am here for vpn%d and numPagesAllocated%d\n",i,numPagesAllocated);
            physvpn[next_page] = i;
            j++;
                DEBUG('o', "I am in child for vpn %d np %d and pnp %d\n",i,next_page,next_page_parent);
            for(int z=0;z<PageSize;z++){
                machine->mainMemory[PageSize*next_page+z] = machine->mainMemory[next_page_parent*PageSize+z];
            }
        }
        else{
            pageTable[i].physicalPage=parentPageTable[i].physicalPage;
    //        DEBUG('k',"Hello %d\n",i);
        }
        pageTable[i].valid = parentPageTable[i].valid;
        pageTable[i].use = parentPageTable[i].use;
        pageTable[i].dirty = parentPageTable[i].dirty;
        pageTable[i].readOnly = parentPageTable[i].readOnly;    // if the code segment was entirely on
        pageTable[i].shared = parentPageTable[i].shared;                                            // a separate page, we could set its
        pageTable[i].backedup= parentPageTable[i].backedup;                                            // pages to be read-only
        for(int k=0;k<PageSize;k++){
            backup[i*PageSize+k]=parentSpace->backup[i*PageSize+k];
        }
    }
}

int 
AddrSpace::replace(int ppn){
    int rnd,temppid,vpn;
    if(algo=='0'){
        rnd = Random()%NumPhysPages;
        while(rnd==ppn || threadArray[physpid[rnd]]->space->pageTable[physvpn[rnd]].shared){
            rnd = Random()%NumPhysPages;
        }
        
    }
    else if(algo=='1'){
        if(fifo->tail->ppn!=ppn){
            rnd = fifo->tail->ppn;
            fifo->bringtotop(fifo->tail);
            physfifo[rnd] = fifo->head;
        }
        else{
            rnd = fifo->tail->prev->ppn;
            fifo->bringtotop(fifo->tail->prev);
            physfifo[rnd] = fifo->head;
        }
        DEBUG('l',"I am here with rnd %d\n",rnd);
    }
    else if(algo=='2'){
        if(lru->tail->ppn!=ppn){
            rnd = lru->tail->ppn;
            lru->bringtotop(lru->tail);
            physlru[rnd] = lru->head;
        }
        else{
            rnd = lru->tail->prev->ppn;
            lru->bringtotop(lru->tail->prev);
            physlru[rnd] = lru->head;
        }
    }
    else if(algo=='3'){
        while(lruclk[lruclkptr]!=0){
            if(lruclk[lruclkptr] == 1 && lruclkptr!=ppn){
                lruclk[lruclkptr] = 0;
            }
            lruclkptr = (lruclkptr+1)%NumPhysPages;
        }
        lruclk[lruclkptr] = 1;
        rnd = lruclkptr;
    }
    temppid = physpid[rnd];
    vpn = physvpn[rnd];
    Thread* tempthread = threadArray[temppid];
    //         DEBUG('y',"Making replacement %d\n",rnd);
    TranslationEntry * pageTable1 = tempthread->space->pageTable;
        DEBUG('y', "Backing up for pid %d and vpn %d\n",temppid,vpn);
    for(int i=0;i<PageSize;i++){
        tempthread->space->backup[vpn*PageSize+i] = machine->mainMemory[rnd*PageSize+i];
    }
    pageTable1[vpn].physicalPage = -1;
    pageTable1[vpn].valid = FALSE;
    pageTable1[vpn].dirty = FALSE;
    return rnd;
}

void
AddrSpace::removepages(){
    if(algo=='0'){
        DEBUG('j', "Getting outof thread\n");
        for(unsigned int i=0;i<numPages;i++){
            if(pageTable[i].valid && !pageTable[i].shared){    
                pagesfree->Append((void*)(pageTable[i].physicalPage));
                numPagesAllocated--;
                free_page_count++;
            }
        }
    }
    else if(algo=='1'){
        for(unsigned int i=0;i<numPages;i++){
            if(pageTable[i].valid && !pageTable[i].shared){    
                pagesfree->Append((void*)(pageTable[i].physicalPage));
                numPagesAllocated--;
                free_page_count++;
                fifo->deletenode(physfifo[pageTable[i].physicalPage]);
            }
        }
    }
    else if(algo=='2'){
        for(unsigned int i=0;i<numPages;i++){
            if(pageTable[i].valid && !pageTable[i].shared){    
                pagesfree->Append((void*)(pageTable[i].physicalPage));
                numPagesAllocated--;
                free_page_count++;
                lru->deletenode(physlru[pageTable[i].physicalPage]);
            }
        }
    }
    else if(algo=='3'){
        for(unsigned int i=0;i<numPages;i++){
            if(pageTable[i].valid && !pageTable[i].shared){    
                pagesfree->Append((void*)(pageTable[i].physicalPage));
                numPagesAllocated--;
                free_page_count++;
                lruclk[pageTable[i].physicalPage] = -2;
            }
        }
    }
}

void
AddrSpace::maintain(int ppn){
    if(algo=='1'){
        fifo->insertathead(fifo->makenode(ppn));
        physfifo[ppn]=(fifo->head);
    }
    else if(algo=='2'){
        lru->insertathead(lru->makenode(ppn));
        physlru[ppn]=(lru->head);
    }
    else if(algo == '3'){
        lruclk[ppn] = 1;
    }
}