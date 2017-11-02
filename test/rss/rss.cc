#include <stdio.h>
#include <random>
#include <string.h>
#include <arpa/inet.h>
#include <inttypes.h>


//NOTE input_len is expressed in 4-bytes units
uint32_t
rte_softrss_be(uint32_t *input_tuple, uint32_t input_len, const uint8_t *rss_key)
{
         uint32_t i, j, ret = 0;
 
         for (j = 0; j < input_len; j++) {
                for (i = 0; i < 32; i++) {
                         if (input_tuple[j] & (1 << (31 - i))) {
                                 ret ^= ((const uint32_t *)rss_key)[j] << i | (uint32_t)((uint64_t)(((const uint32_t *)rss_key)[j + 1]) >> (32 - i));
                        }
                 }
        }
        return ret;
}



int
main(int argc, char **argv){
    //Params
    unsigned int in_len=12;
    unsigned int port_len=2;
    unsigned int key_len=40;
    unsigned int hash_len=4;
    
    //Main
    uint8_t in [in_len];
    uint8_t key [key_len] ;
  
    std::random_device rd;
  
    printf("Test RSS Hash decomposition:\n--------------------------------\n");
    
    //RND inputs
    for (unsigned int i = 0; i<in_len; i++){in[i] = rd();}
    for (unsigned int i = 0; i<key_len; i++){key[i] = rd();}
  
    //Compute Hash RSS at once
    uint32_t hash = rte_softrss_be((uint32_t*)in,in_len/4,key);
  
    //Compute Hash RSS in three times
    uint8_t* in_tmp = in;
    uint32_t h1 = rte_softrss_be((uint32_t*)in_tmp, 1,key); //ip_src
    in_tmp += 4;
    uint32_t h2 = rte_softrss_be((uint32_t*)in_tmp, 1,key+4); //ip_dst
    in_tmp += 4;
    uint32_t h3 = rte_softrss_be((uint32_t*)in_tmp ,1, key+8); //ports

    uint32_t hash2 = 0; 
    hash2 = h1 ^ h3 ^ h2 ;
    
    printf("HASH RSS at once    : ");  
    uint8_t * h = (uint8_t *) &hash;
    for (unsigned int i = hash_len; i>0; i--) printf("%x ",h[i-1]);  
    
    printf("\nHASH RSS in 3 times : ");
    h = (uint8_t *) &hash2;
    for (unsigned int i = hash_len; i>0; i--) printf("%x ",h[i-1]);  
    printf("\n\n");
    
    printf("Test RSS Hash port assign :\n--------------------------------\n");
    
    uint8_t conn1[in_len];
    uint8_t conn2[in_len];
    
    int core_conn1=-1;
    int core_conn2=-1;
    
    uint8_t ncores=40;
    unsigned int nrep = 1000;
    unsigned int tot_rep=0;
    
    
    for (unsigned int j =0; j < nrep; j++){
	//RND inputs
	unsigned int rep=0;
	for (unsigned int i = 0; i<in_len; i++){conn1[i] = rd();}
	for (unsigned int i = 0; i<in_len; i++){conn2[i] = rd();}
	for (unsigned int i = 0; i<key_len; i++){key[i] = rd();}
	
	uint8_t* tmp = conn1;
    
	uint32_t h_conn1 = rte_softrss_be((uint32_t*)tmp, in_len/4,key); 
	uint32_t h_conn2 = 0;
	core_conn1 = h_conn1 % ncores;
	
	
	while(core_conn1 != core_conn2){

	    for (unsigned int i = 0; i<port_len; i++){conn2[i+8] = rd();}//random source port

	    tmp = conn2;
	    h_conn2 = rte_softrss_be((uint32_t*)tmp, in_len/4,key) ;
	    core_conn2 = h_conn2 % ncores;


	    rep++;
	}
	
	printf("%d Repetition %d\n", j, rep);
	tot_rep+=rep;
    }
    
    printf("Avg repetition %f \n",(double)tot_rep/nrep);
    return 0;
    
}