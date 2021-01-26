// Copyright (C) 2021, Gururaj Saileshwar, Moinuddin Qureshi: Georgia Institute of Technology.

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "mtrand.h"

//argv[1]
int SPILL_THRESHOLD = 13;
//argv[2]
int NUM_BILLION_TRIES = 1;

#define BILLION_TRIES             (1000*1000*1000)
#define HUNDRED_MILLION_TRIES     (100*1000*1000)

#define NUM_SKEWS                      (2)
#define BASE_WAYS_PER_SKEW             (8)

//2MB Cache
//#define NUM_BUCKETS_PER_SKEW  (1<<11)
//#define NUM_BUCKETS           (1<<12)

//16 MB
#define NUM_BUCKETS_PER_SKEW  (1<<14)
#define NUM_BUCKETS           (1<<15)

#define BALLS_PER_BUCKET      (8)
#define MAX_FILL              (16)

// Tie-Breaking Policy between Skews.
//0 - Randomly picks either skew on Ties. 
//1 - Always picks Skew-0 on Ties.
#define BREAK_TIES_PREFERENTIALLY      (0)

MTRand *mtrand=new MTRand();

typedef unsigned int uns;
typedef unsigned long long uns64;
typedef double dbl;

uns64 bucket[NUM_BUCKETS];
uns64 balls[NUM_BUCKETS*BALLS_PER_BUCKET];

bool init_buckets_done = false;
uns64 bucket_fill_observed[MAX_FILL+1];
uns64 stat_counts[MAX_FILL+1];

uns64 spill_count = 0;
uns64 cuckoo_spill_count = 0;

/////////////////////////////////////////////////////
//---- Used for relocating a filled bucket ---------
/////////////////////////////////////////////////////

//Based on which skew spill happened; cuckoo into other recursively.
void spill_ball(uns64 index, uns64 ballID){
  uns done=0;

  bucket[index]--;

  while(done!=1){
    //Pick skew & bucket-index where spilled ball should be placed.
    uns64 spill_index ;
    //If current index is in Skew0, then pick Skew1. Else vice-versa.
    if(index < NUM_BUCKETS_PER_SKEW)
      spill_index = NUM_BUCKETS_PER_SKEW + mtrand->randInt(NUM_BUCKETS_PER_SKEW-1);
    else
      spill_index = mtrand->randInt(NUM_BUCKETS_PER_SKEW-1);

    //If new spill_index bucket where spilled-ball is to be installed has space, then done.
    if(bucket[spill_index] < SPILL_THRESHOLD){
      done=1;
      bucket[spill_index]++;
      balls[ballID] = spill_index;
     
    } else {
      assert(bucket[spill_index] == SPILL_THRESHOLD);
      //if bucket of spill_index is also full, then recursive-spill, we call this a cuckoo-spill
      index = spill_index;
      cuckoo_spill_count++;
    }
  }

  spill_count++;
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

uns insert_ball(uns64 ballID){

  uns64 index1 = mtrand->randInt(NUM_BUCKETS_PER_SKEW - 1);
  uns64 index2 = NUM_BUCKETS_PER_SKEW + mtrand->randInt(NUM_BUCKETS_PER_SKEW - 1);

  if(init_buckets_done){
    bucket_fill_observed[bucket[index1]]++;
    bucket_fill_observed[bucket[index2]]++;
  }
    
  uns64 index;
  uns retval;

  if(bucket[index2] < bucket[index1]){
    index = index2;
  } else if (bucket[index1] < bucket[index2]){
    index = index1;    
  } else if (bucket[index2] == bucket[index1]) {

#if BREAK_TIES_PREFERENTIALLY == 0
    //Break ties randomly
    if(mtrand->randInt(1) == 0)
      index = index1;
    else
      index = index2;

#elif BREAK_TIES_PREFERENTIALLY == 1
    //Break ties favoring 0th skew.
    index = index1;
#endif
     
  } else {
    assert(0);
  }
      
  retval = bucket[index];
  bucket[index]++;

  assert(balls[ballID] == (uns64)-1);
  balls[ballID] = index;
  
  //----------- SPILL --------
  if(SPILL_THRESHOLD && (retval >= SPILL_THRESHOLD)){
    //Overwrite balls[ballID] with spill_index.
    spill_ball(index,ballID);   
  }
  
  return retval;  
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

uns64 remove_ball(void){
  uns64 ballID = mtrand->randInt(NUM_BUCKETS*BALLS_PER_BUCKET -1);

  assert(balls[ballID] != (uns64)-1);
  uns64 bucket_index = balls[ballID];
    
  assert(bucket[bucket_index] != 0 );  
  bucket[bucket_index]--;
  balls[ballID] = -1;

  return ballID;
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

void display_histogram(void){
  uns ii;
  uns s_count[MAX_FILL+1];

  for(ii=0; ii<= MAX_FILL; ii++){
    s_count[ii]=0;
  }

  for(ii=0; ii< NUM_BUCKETS; ii++){
    s_count[bucket[ii]]++;
  }

  printf("\n");
  for(ii=0; ii<= MAX_FILL; ii++){
    double perc = 100.0 * (double)s_count[ii]/(double)(NUM_BUCKETS);
    printf("\n Bucket[%2u Fill]: %u (%4.2f)", ii, s_count[ii], perc);
  }

  printf("\n");
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

void sanity_check(void){
  uns ii, count=0;
  uns s_count[MAX_FILL+1];

  for(ii=0; ii<= MAX_FILL; ii++){
    s_count[ii]=0;
  }
  
  for(ii=0; ii< NUM_BUCKETS; ii++){
    count += bucket[ii];
    s_count[bucket[ii]]++;
  }


  if(count != (NUM_BUCKETS*BALLS_PER_BUCKET)){
    printf("\n*** Sanity Check Failed, TotalCount: %u*****\n", count);
  }
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

void init_buckets(void){
  uns64 ii;

  assert(NUM_SKEWS * NUM_BUCKETS_PER_SKEW == NUM_BUCKETS);
  
  for(ii=0; ii<NUM_BUCKETS; ii++){
    bucket[ii]=0;
  }
 

  for(ii=0; ii<(NUM_BUCKETS*BALLS_PER_BUCKET); ii++){
    balls[ii] = -1;
    insert_ball(ii);
  }

  for(ii=0; ii<=MAX_FILL; ii++){
   stat_counts[ii]=0;
  }

  sanity_check();
  init_buckets_done = true;
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

uns  remove_and_insert(void){
  
  uns res = 0;

  uns64 ballID = remove_ball();
  res = insert_ball(ballID);

  if(res <= MAX_FILL){
    stat_counts[res]++;
  }else{
    printf("Overflow\n");
    exit(-1);
  }

  //printf("Res: %u\n", res);
  return res;
}



/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

int main(int argc, char* argv[]){

  //Get arguments:
  assert((argc == 4) && "Need 3 arguments: (EXTRA_BUCKET_CAPACITY:[0-8] BN_BALL_THROWS:[1-10^5] SEED:[1-400])");
  SPILL_THRESHOLD = atoi(argv[1]);
  NUM_BILLION_TRIES  = atoi(argv[2]);
  uns myseed = atoi(argv[3]);

  printf("Running simulation with SPILL_THRESHOLD:%d, NUM_TRIES:%d Billion, Seed:%d\n\n",SPILL_THRESHOLD,NUM_BILLION_TRIES,myseed);
  
  uns64 ii;
  mtrand->seed(myseed);

  init_buckets();
  
  sanity_check();

  printf("Starting --  (Dot printed every 100M Ball throws) \n");

  //N Billion Ball Throws
  for (uns64 bn_i=0 ; bn_i < NUM_BILLION_TRIES; bn_i++) {    
    //1 Billion Ball Throws
    for(uns64 hundred_mn_count=0; hundred_mn_count<10; hundred_mn_count++){
      //In multiples of 100 Million Ball Throws.
      for(ii=0; ii<HUNDRED_MILLION_TRIES; ii++){
        //Insert and Remove Ball
        remove_and_insert();      
      }
      printf(".");fflush(stdout);
    }
    //Ensure number of total balls in all the buckets is constant.
    sanity_check();
    //Print billion count.
    printf(" %dBn\n",bn_i+1);fflush(stdout);    
  }

  printf("\n\nBucket-Fill Snapshot at End\n");
  display_histogram();
  printf("\n\n\n");

  printf("Bucket-Fill Average (Seen by Each Ball)\n");
  for(ii=0; ii<= MAX_FILL; ii++){
    double perc = 100.0 * (double)bucket_fill_observed[ii]/(NUM_SKEWS*(double)NUM_BILLION_TRIES*(double)BILLION_TRIES);
    printf("\n Bucket[%2u Fill]: %llu (%5.3f)", ii, bucket_fill_observed[ii], perc);
  }

  printf("\n\n\n");

  printf("Bucket-Fill Best-of-2 (Seen by Each Ball)\n");
  printf("\n");
  for(ii=0; ii<MAX_FILL; ii++){
    double perc = 100.0*(double)(stat_counts[ii])/(double)((double)NUM_BILLION_TRIES*(double)BILLION_TRIES);
    printf("%2llu:\t %8llu\t (%5.3f)\n", ii, stat_counts[ii], perc);
  }

  printf("\nSpill Count: %llu (%5.3f)\n", spill_count,
         100.0* (double)spill_count/(double)((double)NUM_BILLION_TRIES*(double)BILLION_TRIES));
  printf("\nCuckoo Spill Count: %llu (%5.3f)\n", cuckoo_spill_count,
         100.0* (double)cuckoo_spill_count/(double)((double)NUM_BILLION_TRIES*(double)BILLION_TRIES));

  return 0;
}

