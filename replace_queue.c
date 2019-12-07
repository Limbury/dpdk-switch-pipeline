#include "main.h"

#define SLOW_FLOW 0
#define FAST_FLOW 1


void pkt_enqueue_to_rxing(uint32_t src_port,struct rte_mbuf *pkt){
	struct ipv4_hdr *iphdr;
	int ret = 0;
	uint32_t free_buffer;
	if (RTE_ETH_IS_IPV4_HDR(pkt->packet_type)){ //is ipv4 pkt??
		iphdr = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, sizeof(struct ether_hdr));

	}
	else{
		rte_pktmbuf_free(pkt); //don't handle huge frame
		return ;
	}
	free_buffer = rte_ring_free_count(app.rings_rx[src_port]);
	if(free_buffer < app.burst_size_rx_read ){ //don't have enough free space
		if(iphdr->type_of_service == SLOW_FLOW){//Packet dropped due to queue 
			rte_pktmbuf_free(pkt);		//don't have enough free space
			RTE_LOG(
		            DEBUG, SWITCH,
		            "%s: Packet dropped due to queue length > threshold \n",
		            __func__
		        );
			return ;
		}
		else{	//coming pkt is fast flow
			/*
			struct app_mbuf_array *front_pkt;
			front_pkt = rte_malloc_socket(NULL, sizeof(struct app_mbuf_array),
            				RTE_CACHE_LINE_SIZE, rte_socket_id());
			if (front_pkt == NULL)
        			rte_panic("Worker thread: cannot allocate buffer space\n");
			*/
			struct rte_mbuf *front_pkt;
			ret = rte_ring_sc_dequeue(	//choose a pkt to replace
            			app.addr_rings_rx[src_port],
            			(void **) &front_pkt);
			if (ret == -ENOENT){	// if dont't find
				ret = rte_ring_sp_enqueue(	//try enqueue
                			app.rings_rx[src_port],
                			pkt);
				if(!ret)    // if dont't have space
					rte_pktmbuf_free(pkt);	//release
				return ;  //finished 
			}
			else{
				(*front_pkt) = (*pkt); //replace 
				rte_pktmbuf_free(pkt);	
				RTE_LOG(
		        	    DEBUG, SWITCH,
		        	    "%s: Packet replace due to coming fast flow \n",
		        	    __func__
			        );
				return ; //finished
			}
		}
	}
	else{	//have enough free space
		// put into the rx_ring where another another lcore do forwarding
		ret = rte_ring_sp_enqueue(
                	app.rings_rx[src_port],
                	pkt);
		if(!ret && (iphdr->type_of_service == SLOW_FLOW)){ // add the pointer to addr_queue
			ret = rte_ring_sp_enqueue(
                		app.addr_rings_rx[src_port],
                		pkt);
		}
	}
	
}


int pkt_dequeue_from_rxing(uint32_t src_port,void **pkt){
	int ret;
	if(front == NULL ){
		ret = rte_ring_sc_dequeue(
            		app.addr_rings_rx[src_port],
            		(void **) &front);	
		if(ret != 0)
			front = NULL;
	}
	ret = rte_ring_sc_dequeue(	//dequeue a pkt
            	app.rings_rx[src_port],
            	pkt);
	if (ret == -ENOENT)
        	return -ENOENT;
	if((*pkt) == front ){	//move to the first slow_flow's pkt
		ret = rte_ring_sc_dequeue(
            		app.addr_rings_rx[src_port],
            		(void **) &front);
		if(ret != 0)
			front = NULL;
	}
	return 0;
}
