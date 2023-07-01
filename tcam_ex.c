#include <stdio.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "list_api.h"
#include "queue.h"

#define MAX_TCAM_SIZE 12
#define BATCH_SIZE 10

typedef struct entry_ {
	uint32_t id; 
	uint32_t prio;
} entry_t;

uint32_t head_index =0, tail_index=0;

int tcam_init(entry_t *hw_tcam, uint32_t size, void **tcam);

int tcam_insert(void *tcam, entry_t *entries, uint32_t num);

int tcam_remove(void *tcam, uint32_t id);

void verify_tcam_rules(void *tcam);

static entry_t hw_tcam[MAX_TCAM_SIZE];
static uint64_t hw_access;
static uint32_t access_count ;
    
/* init Queue to keep track of order of entries added */
Queue* queue ;;

int tcam_program(entry_t *hw_tcam, entry_t *ent, uint32_t position)
{
	if ( hw_tcam == NULL || ent == NULL || position >= MAX_TCAM_SIZE) {
		printf("Invalid arguments\n");  
		return EINVAL;
	}
	memcpy(hw_tcam + position, ent, sizeof(entry_t));
	hw_access ++;
	return 0;
}

int tcam_init(entry_t *hw_tcam, uint32_t size, void **tcam){

	if ( hw_tcam == NULL || size == 0 || tcam == NULL ) {
		return EINVAL;
	}

	memset(hw_tcam,0,size*sizeof(entry_t));

	/*set the tcam hanlder pointer*/ 
	*tcam = hw_tcam;
	return 0;

}

/* 
 * tcam_insert
 *entries -> incoming batch
 *nums -> batch size
 */

int tcam_insert(void *tcam, entry_t *entries, uint32_t num){
	/*input args error handling*/
	if (tcam == NULL || entries == NULL || num == 0) {
		return EINVAL;
	}

	entry_t *hw_tcam = (entry_t *) tcam;
    int rc = 0;
	/*    
	  uint32_t unique_id = 1;  
	  assuming the incoming batch will have unique IDs , 
	  otherwise  we need to monotonically increase this unique ID and 
	  assign it to the entry ID 
    */    
	uint32_t insert_position = tail_index;

	/* 
	   find the correct position to insert , maintain a list of free positions 
	   ,extra space but faster lookup
	 */ 
	bool used_free_segment = 0;

	/*
	   sanity check on Id's , ID can not be zero , 
	   rolling back the already programmed entries needs HW access , 
	   so checking here to stop the batch porgramming if ID is zero
	 */
	for (uint32_t i = 0; i < num; i++) {
		/* ID can not be Zero*/
		if(entries[i].id==0){
			printf("ID can not be zero , Don't insert the batch\n");
			return 0;
		}
	}

	/* check the batch size , if its larger than limit*/
	if(num > BATCH_SIZE){
		printf("\n Batch Size Exceeded \n");
		return 0;
	} 

	/*
	   program the batch, loop through the entries and program them following
	   priority and location rules 
	 */
	int deleted_id;
	int count = num;
	/* Check if the TCAM table is full and delete older entries*/
	if (insert_position >= MAX_TCAM_SIZE && free_fragment_list_size()<num) {
		while(count){
			deleted_id = dequeue(queue);
			rc=tcam_remove(hw_tcam,deleted_id);
			count--;
		}
		//break;
	}

	for (uint32_t i = 0; i < num; i++) {

		/*
		   Check if there is a suitable free fragment in the list 
		   for insertion
		 */
		free_fragment_t *free_fragment = find_free_fragment(1);
		if (free_fragment != NULL) {
			insert_position = free_fragment->start;
		} else {
			insert_position = tail_index;

		}

		/*
		   Here we got the free index from the list , starting from that position 
		   find the correct position to insert satisfying the priority/location 
		   conditions , we may or maynot insert into the free position we got 
		   from list , so keep track of it and remove it from the free segment list 
		   only if we use it 
		 */
		uint32_t actual_pos = insert_position;
		while (actual_pos > 0 && (entries[i].prio < hw_tcam[actual_pos - 1].prio ||
					(entries[i].prio == hw_tcam[actual_pos - 1].prio))) {
			/* Shift the entries to the bottom to make room for the new entry*/
			memcpy(&hw_tcam[actual_pos], &hw_tcam[actual_pos - 1], sizeof(entry_t));
			access_count++;
			actual_pos--;

		}
		/* handling inserting to Index 0 of Tcam */
		if(actual_pos == 0 && tail_index!=0){
			if(entries[i].prio <= hw_tcam[actual_pos + 1].prio){
				int ret = tcam_program(hw_tcam, &entries[i], actual_pos);
				tail_index++;
                enqueue(queue, entries[i].id);
				access_count++;
				if(free_fragment && free_fragment->start == actual_pos){
					/* we are actaully using the free fragment , so free it up*/       
					remove_free_fragment(free_fragment);
					tail_index--;
				} 
				continue;
			} else if(entries[i].prio > hw_tcam[actual_pos + 1].prio){
				/*if we can't use the free position which we got from list ,
				  start from end to find the right position
				 */
				actual_pos = tail_index;
				while(entries[i].prio <= hw_tcam[actual_pos - 1].prio){
					memcpy(&hw_tcam[actual_pos], &hw_tcam[actual_pos - 1], sizeof(entry_t));
					access_count++;
					actual_pos--;
				}

			}
		}

		if(free_fragment && free_fragment->start == actual_pos){
			/*  we are actaully using the free fragment , so free it up*/       
			remove_free_fragment(free_fragment);
			used_free_segment = 1;
		}

		/* Program the entry into the TCAM*/
		int ret = tcam_program(hw_tcam, &entries[i], actual_pos);
		if(ret !=0){
			printf("TCAM Program Failed");
			goto cleanup;
		}
        enqueue(queue, entries[i].id);
		access_count++;

		/* if we did not use the free entry from list  , means 
		   we used entry at tail index
		 */
		if(used_free_segment==0) tail_index++;

		/* reset it*/
		used_free_segment=0;

		continue;
cleanup:
		/*need to modify to handle entire batch 
		  Rollback the TCAM state
		 */
		for (uint32_t j = 0; j < i; j++) {
			if (hw_tcam[actual_pos + j].id == entries[j].id) {
				memset(&hw_tcam[actual_pos + j], 0, sizeof(entry_t));
			}
		}

	}

	return 0;
}

bool is_tcam_empty(){
	if((head_index == tail_index) && free_fragment_list_size()==0) return true;
	else return false ;

}

/*tcam entry remove*/

int tcam_remove(void *tcam, uint32_t id) {
	if (tcam == NULL) {
		return EINVAL;
	}
	/* check if TCAM is empty */
	if(is_tcam_empty()){
		printf("Tcam empty \n");
		return 1;
	}
	entry_t *hw_tcam = (entry_t *)tcam;

	for (uint32_t i = 0; i < MAX_TCAM_SIZE; i++) {
		if (hw_tcam[i].id == id && (id!=0)) {
			/* Remove the entry from the TCAM*/
			memset(&hw_tcam[i], 0, sizeof(entry_t));

			/*Add the freed fragment to the free fragment list */
			add_free_fragment(i, i);

			return 0;
		}
	}

	return ENOENT;
}

int reset_tcam(void* tcam){
	if (tcam == NULL) {
		return EINVAL;
	}
	entry_t *hw_tcam = (entry_t *)tcam;
	for (uint32_t i = 0; i < MAX_TCAM_SIZE; i++) {
		memset(&hw_tcam[i], 0, sizeof(entry_t));
	}
	destroy_free_fragments();
	tail_index = 0;
	return 0;
}

/* verify the correctness of TCAM */

void verify_tcam_rules(void *tcam) {
	entry_t *hw_tcam = (entry_t *)tcam;
	uint32_t prev_prio = 0;
	uint32_t prev_id = 0;
	bool rule_met = true;

	prev_prio= hw_tcam[0].prio;
	prev_id = hw_tcam[0].id;
	for (uint32_t i = 1; i < MAX_TCAM_SIZE; i++) {
		if (hw_tcam[i].id != 0 ) {
			if (hw_tcam[i].prio < prev_prio) {
				rule_met = false;
				break;
			} else if (hw_tcam[i].prio == prev_prio && hw_tcam[i].id > prev_id) {
				rule_met = false;
				break;
			}

			prev_prio = hw_tcam[i].prio;
			prev_id = hw_tcam[i].id;
		}
	}

	printf("\nPriority and location rules are met: %s\n", rule_met ? "Yes" : "No");
}

/*print contents of Tcam*/
void print_tcam(){
	printf("Tcam Contents \n");
	for (uint32_t i = 0; i < MAX_TCAM_SIZE; i++) {
		printf("ID: %u, Priority: %u\n", hw_tcam[i].id, hw_tcam[i].prio);
	}

}


int main(){

	void* tcam ;
	int rc = 0;
	entry_t entries[BATCH_SIZE+1];

	printf("\n\nTEST CASE 0 : Invalid Args  \n");
	entries[0] = (entry_t){.id = 13, .prio = 200};
	entries[1] = (entry_t){.id = 13, .prio = 200};
	entries[2] = (entry_t){.id = 13, .prio = 200};
	rc= tcam_insert(NULL,entries,11);
	if(rc!=0){
		printf(" Tcam Insert failed\n");
	}

	//tcam init
	printf(" \n\nTEST CASE 1:test init failure error handling\n");
	rc = tcam_init(NULL,MAX_TCAM_SIZE,&tcam);
	//error checking
	if(rc != 0){
		printf("Tcam Init failed \n");
	} else {
		printf("Tcam Init Success \n");
	}


	printf("\n\nTEST CASE 2: test init Success\n");
	rc = tcam_init(hw_tcam,MAX_TCAM_SIZE,&tcam);
	if(rc != 0){
		printf("\nTcam Init failed \n");
	} else {
		printf("Tcam Init Success \n");
	}
	/* free fragments linked list */
	free_fragments = NULL;
    
    /* init Queue to keep track of order of entries added */
    queue = createQueue();
    
    int deleted_id;
	printf("\n\nTEST CASE 3 : Remove an element when TCAM is empty\n");
	rc=tcam_remove(tcam,12);
	if(rc!=0){
		printf("Tcam Remove  failed\n");
	} else deleteNode(queue,13);

	printf("\n\nTEST CASE 4: Insert entries with unique priorities\n");
	/*create an batch*/
	entries[0] = (entry_t){.id = 1, .prio = 100};
	entries[1] = (entry_t){.id = 2, .prio = 200};
	entries[2] = (entry_t){.id = 3, .prio = 300};

	printf("Inserting batch 1  TCAM entries:\n");
	/*insert into tcam*/
	rc= tcam_insert(tcam,entries,3);
	if(rc!=0){
		printf("Tcam Insert failed\n");
	}
	print_tcam();

	printf("total HW access after batch 1 %d \n",access_count);

	printf("\n\nTEST CASE 5: Insert entries with  same priorities which alredy exist in TCAM\n");

	entries[0] = (entry_t){.id = 4, .prio = 100};
	entries[1] = (entry_t){.id = 5, .prio = 200};
	entries[2] = (entry_t){.id = 6, .prio = 300};
	printf("\nInserting  batch 2 TCAM entries:\n");
	rc= tcam_insert(tcam,entries,3);

	if(rc!=0){
		printf("Tcam Insert failed\n");
	}
	print_tcam();
	printf("total HW access  %d \n", access_count);

	printf("\n\nTEST CASE 6: Remove an element \n");
	printf("remove ID 4:\n");
	rc = tcam_remove(tcam ,4);

	if(rc!=0){
		printf("Tcam Remove  failed\n");
	} else deleteNode(queue,4);
	print_tcam(); 

	/*add batch 3*/
	printf("\n\nTEST CASE 6: Add another batch of unique and same priorities\n");
	entries[0] = (entry_t){.id = 7, .prio = 500};
	entries[1] = (entry_t){.id = 8, .prio = 300};
	printf("Inserting batch 3 : TCAM entries :\n");
	rc= tcam_insert(tcam,entries,2);
	if(rc!=0){
		printf("\nTcam Insert failed\n");
	}
	print_tcam(); 

	/*add batch 4*/
	printf("\n\nTEST CASE 7: Add another batch of same priorities of smallest batch size 1\n");
	entries[0] = (entry_t){.id = 10, .prio = 100};
	rc= tcam_insert(tcam,entries,1);
	printf("Inserting batch 4 :TCAM entries:\n");
	if(rc!=0){
		printf("Tcam Insert failed\n");
	}
	print_tcam(); 

	/*add batch 5*/
	printf("\n\nTEST CASE 8: Add another batch of same priorities\n");
	entries[0] = (entry_t){.id = 12, .prio = 100};
	entries[1] = (entry_t){.id = 13, .prio = 200};
	printf("Inserting batch 5 :TCAM entries:\n");
	rc= tcam_insert(tcam,entries,2);
	if(rc!=0){
		printf("\nTcam Insert failed\n");
	}
	print_tcam(); 


	/*remove 13*/
	printf("\n\nTEST CASE 9: Remove element from middle of TCAM\n");
	printf("remove ID 13:\n");
	rc = tcam_remove(tcam ,13);
	if(rc!=0){
		printf("Tcam Remove failed %d\n",rc);
	} else deleteNode(queue,13);
	print_tcam(); 

	/*add batch 6*/
	printf("\n\nTEST CASE 10 : Negative test : Add entry with ID 0 \n");
	printf("Inserting batch 6 :TCAM entries:\n");
	entries[0] = (entry_t){.id = 13, .prio = 200};
	entries[1] = (entry_t){.id = 15, .prio = 200};
	entries[2] = (entry_t){.id = 0, .prio = 0};
	entries[3] = (entry_t){.id = 17, .prio = 0};
	rc= tcam_insert(tcam,entries,4);
	if(rc!=0){
		printf("Tcam Insert failed\n");
	}
	print_tcam(); 

	/*add batch 7*/
	printf("\n\nTEST CASE 11 : Add elements including one with  highest priority Zero \n");
	printf("inserting batch 7 :TCAM entries:\n");
	entries[0] = (entry_t){.id = 13, .prio = 200};
	entries[1] = (entry_t){.id = 15, .prio = 200};
	entries[2] = (entry_t){.id = 17, .prio = 0};
	rc= tcam_insert(tcam,entries,3);
	if(rc!=0){
		printf("\nTcam Insert failed\n");
	}
	print_tcam(); 

	/* add batch 8 */
	printf("\n\nTEST CASE 12 : TCAM FULL: Removing Older entries and adding Newer entries \n ");
	printf("inserting batch 8 :TCAM entries:\n");
	entries[0] = (entry_t){.id = 18, .prio = 200};
	entries[1] = (entry_t){.id = 19, .prio = 200};
	rc= tcam_insert(tcam,entries,2);
	if(rc!=0){
		printf(" 12:Tcam Insert failed\n");
	}

	/*remove Non existent entry*/
	printf("\n\nTEST CASE 13 : Remove a non existing element with ID 50\n");
	rc = tcam_remove(tcam ,50);
	if(rc!=0){
		if(rc==ENOENT){
			printf("Entry Doesn't exist, ");
		}
		printf("Tcam Remove failed \n");
	} else deleted_id = dequeue(queue);
	print_tcam(); 

	/*verify the tcam , if entries are as per priority rules*/
	printf("\n\nTEST CASE 14 : Validate TCAM  \n");
	verify_tcam_rules(tcam);   

	printf("total HW access  %d \n", access_count);

	// Reset Tcam 
	printf("\n\nTEST CASE 15 Reset TCAM \n");
	rc = reset_tcam(tcam);
	if(rc!=0){
		printf(" Tcam Reset failed\n");
	}
	print_tcam();

	// Sending batch size larger than Limit 
	printf("\n\nTEST CASE 16 Sending batch larger than Max Batch size  \n");
	entries[0] = (entry_t){.id = 13, .prio = 200};
	entries[1] = (entry_t){.id = 1, .prio = 200};
	entries[2] = (entry_t){.id = 2, .prio = 200};
	entries[3] = (entry_t){.id = 3, .prio = 200};
	entries[4] = (entry_t){.id = 15, .prio = 200};
	entries[5] = (entry_t){.id = 17, .prio = 0};
	entries[6] = (entry_t){.id = 10, .prio = 200};
	entries[7] = (entry_t){.id = 11, .prio = 0};
	entries[8] = (entry_t){.id = 20, .prio = 200};
	entries[9] = (entry_t){.id = 27, .prio = 0};
	entries[10] = (entry_t){.id = 25, .prio = 200};
	rc= tcam_insert(tcam,entries,11);
	if(rc!=0){
		printf(" Tcam Insert failed\n");
	}


	printf("\n\nTEST CASE 17 : Inserting after TCAM RESET  \n");
	entries[0] = (entry_t){.id = 13, .prio = 200};
	entries[1] = (entry_t){.id = 11, .prio = 400};
	rc= tcam_insert(tcam,entries,2);
	if(rc!=0){
		printf(" Tcam Insert failed\n");
	}
	print_tcam();
	return 0;
}
