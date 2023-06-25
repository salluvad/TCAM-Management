#include <stdio.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "list_api.h"

typedef struct entry_ {
    uint32_t id; 
    uint32_t prio;
} entry_t;

uint32_t head_index =0, tail_index=0;
int tcam_init(entry_t *hw_tcam, uint32_t size, void **tcam);

int tcam_insert(void *tcam, entry_t *entries, uint32_t num);

int tcam_remove(void *tcam, uint32_t id);

void verify_tcam_rules(void *tcam);

static entry_t hw_tcam[2048];
static uint64_t hw_access;
static uint32_t access_count ;

int tcam_program(entry_t *hw_tcam, entry_t *ent, uint32_t position)
{
    if (position >= 2048) return EINVAL;
    memcpy(hw_tcam + position, ent, sizeof(entry_t));
    hw_access ++;
    return 0;
}

int tcam_init(entry_t *hw_tcam, uint32_t size, void **tcam){
    //argument validation

    //
    memset(hw_tcam,0,size*sizeof(entry_t));

    //set the tcam hanlder pointer 
    *tcam = hw_tcam;
    return 0;

}

/* 
 * tcam_insert
 *entries -> incoming batch
 *nums -> batch size
 */

int tcam_insert(void *tcam, entry_t *entries, uint32_t num){
    //input args error handling

    entry_t *hw_tcam = (entry_t *) tcam;

    /*    
      uint32_t unique_id = 1;  // assuming the incoming batch will have unique IDs , otherwise  we need to monotonically increase this unique ID and assign it to the netry ID 
    */    
    uint32_t insert_position = tail_index;
    uint32_t free_index = 0;

    while(hw_tcam[insert_position].id!=0 && insert_position<2048){
        insert_position++;
    }

    //No available space in Tcam
    if(insert_position == 2048  && free_fragment_list_size()==0){
        return ENOSPC;
    }

    //check if we can insert the whole batch 
    if(insert_position+num > 2048 &&free_fragment_list_size()<num){
        return ENOSPC;
    }

    //sort the array ??? this needs a lot of hw_access ????

    // find the correct position to insert , maintain a list of free positions , extra space but faster lookup 
    bool table_full = false ;

    for (uint32_t i = 0; i < num; i++) {
        
      // Check if the TCAM table is full
        if (insert_position >= 2048 && free_fragment_list_size()<num) {
            table_full = true;
            break;
        }

        // Check if there is a suitable free fragment for insertion
        free_fragment_t *free_fragment = find_free_fragment(1);
        if (free_fragment != NULL) {
            insert_position = free_fragment->start;
            free_index = insert_position;
            printf("FREE LOC: %d \n ",insert_position);
            //remove_free_fragment(free_fragment);
        } else {
            insert_position = tail_index;

        }

        //find the correct insert position satisfying the priority/location condition
        uint32_t actual_pos = insert_position;
        while (actual_pos > 0 && (entries[i].prio < hw_tcam[actual_pos - 1].prio ||
                    (entries[i].prio == hw_tcam[actual_pos - 1].prio))) {
            // Shift the entries to the bottom to make room for the new entry
            memcpy(&hw_tcam[actual_pos], &hw_tcam[actual_pos - 1], sizeof(entry_t));
//            printf("in while1 insert_pos %d  and  act pos %d and tail %d\n",insert_position,actual_pos,tail_index);
            access_count++;
            actual_pos--;


        }

        if(actual_pos == 0 && tail_index!=0){
            if(entries[i].prio <= hw_tcam[actual_pos + 1].prio){
                int ret = tcam_program(hw_tcam, &entries[i], actual_pos);
                tail_index++;
                access_count++;
             //   printf("inserting at Zero %d \n",actual_pos);
                   if(free_fragment && free_fragment->start == actual_pos){
               //       printf("Deleting Free node for %d\n", actual_pos);
                      remove_free_fragment(free_fragment);
                // we are actaully using the free fragment , so free it up       
                     tail_index--;
                   } 
              continue;
            } else if(entries[i].prio > hw_tcam[actual_pos + 1].prio){
                actual_pos = tail_index;
                while(entries[i].prio <= hw_tcam[actual_pos - 1].prio){
                    memcpy(&hw_tcam[actual_pos], &hw_tcam[actual_pos - 1], sizeof(entry_t));
                 access_count++;
                    actual_pos--;
                }

            }
        }

         if(free_fragment && free_fragment->start == actual_pos){
          // printf(" 2:Deleting Free node for %d\n", actual_pos);
           remove_free_fragment(free_fragment);
        // we are actaully using the free fragment , so free it up       
        } else {
       // add_free_fragment(insert_position,insert_position);
        }

         // Program the entry into the TCAM
        int ret = tcam_program(hw_tcam, &entries[i], actual_pos);
        access_count++;
        tail_index++;

        //need to modify to handle entire batch 
        if (ret != 0) {
            // Rollback the TCAM state
            for (uint32_t j = 0; j < i; j++) {
                if (hw_tcam[actual_pos + j].id == entries[j].id) {
                    memset(&hw_tcam[actual_pos + j], 0, sizeof(entry_t));
                }
            }
            return ret;
        }

    }

    return table_full ? ENOSPC : 0;
}
//tcam entry remove

int tcam_remove(void *tcam, uint32_t id) {
    if (tcam == NULL) {
        return EINVAL;
    }

    entry_t *hw_tcam = (entry_t *)tcam;

    for (uint32_t i = 0; i < 2048; i++) {
        if (hw_tcam[i].id == id) {
            // Remove the entry from the TCAM
            memset(&hw_tcam[i], 0, sizeof(entry_t));

            // Add the freed fragment to the free fragment list
            add_free_fragment(i, i);

            return 0;
        }
    }

    return ENOENT;
}
//verifer 
void verify_tcam_rules(void *tcam) {
    entry_t *hw_tcam = (entry_t *)tcam;
    uint32_t prev_prio = 0;
    uint32_t prev_id = 0;
    bool rule_met = true;

    prev_prio= hw_tcam[0].prio;
    prev_id = hw_tcam[0].id;
    for (uint32_t i = 1; i < 2048; i++) {
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

    printf("Priority and location rules are met: %s\n", rule_met ? "Yes" : "No");
}

// 
int main(){

    void* tcam ;
    int rc ;

    //tcam init
    rc = tcam_init(hw_tcam,2048,&tcam);
    //error checking
    if(rc == 0){

    }
    //linked list init
    free_fragments = NULL;
    //create an entry
    entry_t entries[3] = {
        {1, 100},   
        {2, 200},  
        {3, 300}   
    };
    //insert into tcam
    rc= tcam_insert(tcam,entries,3);
    for (uint32_t i = 0; i < 15; i++) {
        printf("ID: %u, Priority: %u\n", hw_tcam[i].id, hw_tcam[i].prio);
    }
    
    printf("total HW access after batch 1 %d \n",access_count);
    if(rc==0){
        //error check
    }

    entry_t entries_2[3] = {
        {4, 100},   
        {5, 200},  
        {6, 300}   
    };

    rc= tcam_insert(tcam,entries_2,3);

    printf("\nafter 2 batches TCAM entries:\n");
    for (uint32_t i = 0; i < 15; i++) {
        printf("ID: %u, Priority: %u\n", hw_tcam[i].id, hw_tcam[i].prio);
    }
    printf("\ntotal HW access  %d \n", access_count);
    //remove
    rc = tcam_remove(tcam ,4);

    printf("\nafter remove ID 4:\n");
    for (uint32_t i = 0; i < 15; i++) {
        printf("ID: %u, Priority: %u\n", hw_tcam[i].id, hw_tcam[i].prio);
    }

    //add batch 3
    entry_t entries_3[2] = {{7,500},{8,300} };
    rc= tcam_insert(tcam,entries_3,2);

    //print tcam
    printf("\nafter batch 3 : TCAM entries :\n");
    for (uint32_t i = 0; i < 15; i++) {
        printf("ID: %u, Priority: %u\n", hw_tcam[i].id, hw_tcam[i].prio);
    }

    //add batch 4
    entry_t entries_4[1] = {{10,100} };
    rc= tcam_insert(tcam,entries_4,1);

    //print tcam
    printf("\n after batch 4 :TCAM entries:\n");
    for (uint32_t i = 0; i < 15; i++) {
        printf("ID: %u, Priority: %u\n", hw_tcam[i].id, hw_tcam[i].prio);
    } 
    
    // add batch 5
    entry_t entries_5[2] = {{12,100},{13,200}};
    rc= tcam_insert(tcam,entries_5,2);
    //print tcam
    printf("\n\n after batch 5 :TCAM entries:\n");
    for (uint32_t i = 0; i < 15; i++) {
        printf("ID: %u, Priority: %u\n", hw_tcam[i].id, hw_tcam[i].prio);
    }

    
    //remove 13
    rc = tcam_remove(tcam ,13);
    // print tcam
    printf("\n after remove ID 13:\n");
    for (uint32_t i = 0; i < 15; i++) {
        printf("ID: %u, Priority: %u\n", hw_tcam[i].id, hw_tcam[i].prio);
    }

    // add 13 again 

    // add batch 6
    entry_t entries_6[1] = {{13,200}};
    rc= tcam_insert(tcam,entries_6,1);
    //print tcam
    printf("\n after batch 6 :TCAM entries:\n");
    for (uint32_t i = 0; i < 15; i++) {
        printf("ID: %u, Priority: %u\n", hw_tcam[i].id, hw_tcam[i].prio);
    }

    //verify the tcam , if entries are as per priority rules 
    verify_tcam_rules(tcam);   

    printf("total HW access  %d \n", access_count);
    return 0;
}
